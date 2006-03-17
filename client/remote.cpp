/*
    This file is part of Icecream.

    Copyright (c) 2004 Stephan Kulow <coolo@suse.de>
                  2002, 2003 by Martin Pool <mbp@samba.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>


#ifdef __FreeBSD__
// Grmbl  Why is this needed?  We don't use readv/writev
#include <sys/uio.h>
#endif

#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <map>
#include <algorithm>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <comm.h>
#include "client.h"
#include "tempfile.h"
#include "md5.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

using namespace std;

Environments parse_icecc_version(const string &target_platform )
{
    Environments envs;

    string icecc_version = getenv( "ICECC_VERSION");
    assert( !icecc_version.empty() );

    // free after the C++-Programming-HOWTO
    string::size_type lastPos = icecc_version.find_first_not_of(',', 0);
    string::size_type pos     = icecc_version.find_first_of(',', lastPos);

    list<string> platforms;

    while (pos != string::npos || lastPos != string::npos)
    {
        string couple = icecc_version.substr(lastPos, pos - lastPos);
        string platform = target_platform;
        string version = couple;
        string::size_type colon = couple.find( ':' );
        if (  colon != string::npos ) {
            platform = couple.substr( 0, colon );
            version = couple.substr( colon + 1, couple.length() );
        }

        // Skip delimiters.  Note the "not_of"
        lastPos = icecc_version.find_first_not_of(',', pos);
        // Find next "non-delimiter"
        pos = icecc_version.find_first_of(',', lastPos);

        if (find(platforms.begin(), platforms.end(), platform) != platforms.end()) {
		log_error() << "there are two environments for platform " << platform << " - ignoring " << version << endl;
		continue;
        }

        trace() << "platform '" << platform << "' '" << version << "'" << endl;

        if ( ::access( version.c_str(), R_OK ) ) {
            log_error() << "$ICECC_VERSION has to point to an existing file to be installed " << version << endl;
            throw ( 3 );
        }

        struct stat st;
        if ( lstat( version.c_str(), &st ) || !S_ISREG( st.st_mode )) {
            log_error() << "$ICECC_VERSION has to point to an existing file to be installed " << version << endl;
            throw ( 3 );
        }

        envs.push_back(make_pair( platform, version ) );
        platforms.push_back(platform);

    }

    return envs;
}

Environments rip_out_paths( const Environments &envs, map<string, string> &version_map, map<string, string> &versionfile_map )
{
    version_map.clear();

    string suff = ".tar.bz2";
    Environments env2;

    for ( Environments::const_iterator it = envs.begin(); it != envs.end(); ++it )
    {
        string versfile = it->second;
        if ( versfile.size() > suff.size() && versfile.substr( versfile.size() - suff.size() ) == suff )
        {
            versionfile_map[it->first] = it->second;
            versfile = find_basename( versfile.substr( 0, versfile.size() - suff.size() ) );
            version_map[it->first] = versfile;
            env2.push_back( make_pair( it->first, versfile ) );
        }
    }
    return env2;
}


string get_absfilename( const string &_file )
{
    string file;

    if ( _file.empty() )
        return _file;

    if ( _file.at( 0 ) != '/' ) {
        static char buffer[PATH_MAX];

#ifdef HAVE_GETCWD
        getcwd(buffer, sizeof( buffer ) );
#else
        getwd(buffer);
#endif
        buffer[PATH_MAX - 1] = 0;
        file = string( buffer ) + '/' + _file;
    } else {
        file = _file;
    }

    string::size_type idx = file.find( "/.." );
    while ( idx != string::npos ) {
        file.replace( idx, 3, "/" );
        idx = file.find( "/.." );
    }
    idx = file.find( "/." );
    while ( idx != string::npos ) {
        file.replace( idx, 2, "/" );
        idx = file.find( "/." );
    }
    idx = file.find( "//" );
    while ( idx != string::npos ) {
        file.replace( idx, 2, "/" );
        idx = file.find( "//" );
    }
    return file;
}

static UseCSMsg *get_server( MsgChannel *scheduler )
{
    Msg * umsg = scheduler->get_msg(4 * 60);
    if (!umsg || umsg->type != M_USE_CS)
    {
        log_warning() << "replied not with use_cs " << ( umsg ? ( char )umsg->type : '0' )  << endl;
        delete umsg;
        throw( 1 );
    }
    UseCSMsg *usecs = dynamic_cast<UseCSMsg *>(umsg);
    return usecs;
}

static void write_server_cpp(int cpp_fd, MsgChannel *cserver)
{
    unsigned char buffer[100000]; // some random but huge number
    off_t offset = 0;

    do
    {
        ssize_t bytes = read(cpp_fd, buffer + offset, sizeof(buffer) - offset );
        offset += bytes;
        if (!bytes || offset == sizeof( buffer ) )
        {
            if ( offset )
            {
                FileChunkMsg fcmsg( buffer, offset );
                if ( !cserver->send_msg( fcmsg ) )
                {
                    log_info() << "write of source chunk failed " << offset << " " << bytes << endl;
                    close( cpp_fd );
                    throw( 11 );
                }
                offset = 0;
            }
            if ( !bytes )
                break;
        }
    } while ( 1 );

    close( cpp_fd );
}


static int build_remote_int(CompileJob &job, UseCSMsg *usecs, const string &environment, const string &version_file,
                            const char *preproc_file, bool output )
{
    string hostname = usecs->hostname;
    unsigned int port = usecs->port;
    int job_id = usecs->job_id;
    bool got_env = usecs->got_env;
    job.setJobID( job_id );
    job.setEnvironmentVersion( environment ); // hoping on the scheduler's wisdom
    trace() << "Have to use host " << hostname << ":" << port << " - Job ID: " << job.jobID() << " - environment: " << usecs->host_platform << "\n";

    int status = 255;

    MsgChannel *cserver = 0;

    try {
         cserver = Service::createChannel(hostname, port, 10);
    if ( !cserver ) {
        log_warning() << "no server found behind given hostname " << hostname << ":" << port << endl;
        throw ( 2 );
    }

    unsigned char buffer[100000]; // some random but huge number

    trace() << "got environment " << ( got_env ? "true" : "false" ) << endl;

    off_t offset = 0;

    if ( !got_env ) {
        // transfer env
        struct stat buf;
        if ( stat( version_file.c_str(), &buf ) ) {
            log_perror( "error stat'ing version file" );
            throw( 4 );
        }

        FILE *file = fopen( version_file.c_str(), "rb" );
        if ( !file )
            throw( 5 );

        EnvTransferMsg msg( job.targetPlatform(), job.environmentVersion() );
        if ( !cserver->send_msg( msg ) )
            throw( 6 );

        offset = 0;

        do {
            ssize_t bytes = fread(buffer + offset, 1, sizeof(buffer) - offset, file );
            offset += bytes;
            if (!bytes || offset == sizeof( buffer ) ) {
                if ( offset ) {
                    FileChunkMsg fcmsg( buffer, offset );
                    if ( !cserver->send_msg( fcmsg ) ) {
                        log_info() << "write of source chunk failed " << offset << " " << bytes << endl;
                        fclose( file );
                        throw( 7 );
                    }
                    offset = 0;
                }
                if ( !bytes )
                    break;
            }
        } while (1);
        fclose( file );

        if ( !cserver->send_msg( EndMsg() ) ) {
            log_info() << "write of end failed" << endl;
            throw( 8 );
        }
    }

    CompileFileMsg compile_file( &job );
    if ( !cserver->send_msg( compile_file ) ) {
        log_info() << "write of job failed" << endl;
        throw( 9 );
    }

    offset = 0;

    if ( !preproc_file ) {
        int sockets[2];
        if (pipe(sockets)) {
            /* for all possible cases, this is something severe */
            exit(errno);
        }

	/* This will fork, and return the pid of the child.  It will not
	   return for the child itself.  If it returns normally it will have
	   closed the write fd, i.e. sockets[1].  */
        pid_t cpp_pid = call_cpp(job, sockets[1], sockets[0] );
        if ( cpp_pid == -1 )
            throw( 10 );

        try {
            write_server_cpp( sockets[0], cserver );
        } catch ( int error ) {
            kill( cpp_pid, SIGTERM );
            throw ( error );
        }

        waitpid( cpp_pid, &status, 0);

        if ( status ) { // failure
            delete cserver;
            cserver = 0;
            return WEXITSTATUS( status );
        }
    } else {
        int cpp_fd = open( preproc_file, O_RDONLY );
        if ( cpp_fd < 0 )
            throw ( 11 );

        write_server_cpp( cpp_fd, cserver );
    }

    if ( !cserver->send_msg( EndMsg() ) ) {
        log_info() << "write of end failed" << endl;
        throw( 12 );
    }

    Msg *msg = cserver->get_msg( 12 * 60 );
    if ( !msg )
        throw( 14 );

    if ( msg->type != M_COMPILE_RESULT ) {
        delete msg;
        throw( 13 );
    }

    CompileResultMsg *crmsg = dynamic_cast<CompileResultMsg*>( msg );
    assert ( crmsg );

    status = crmsg->status;
    trace() << "crmsg->status " << status << " " << crmsg->was_out_of_memory << endl;

    if ( status && crmsg->was_out_of_memory ) {
        delete crmsg;
        log_info() << "the server ran out of memory, recompiling locally" << endl;
        throw( 17 ); // recompile locally
    }

    if ( output )
    {
        fprintf( stdout, "%s", crmsg->out.c_str() );
        fprintf( stderr, "%s", crmsg->err.c_str() );

        if ( status && ( crmsg->err.length() || crmsg->out.length() ) )
        {
            fprintf( stderr, "ICECREAM[%d]: Compiled on %s\n", getpid(), hostname.c_str() );
        }
    }
    delete crmsg;

    assert( !job.outputFile().empty() );

    if( status == 0 ) {
        string tmp_file = job.outputFile() + "_icetmp";
        int obj_fd = open( tmp_file.c_str(), O_CREAT|O_TRUNC|O_WRONLY|O_LARGEFILE, 0666 );

        if ( obj_fd == -1 ) {
            log_error() << "open failed\n";
            return EXIT_DISTCC_FAILED;
        }

        msg = 0;
        while ( 1 ) {
            delete msg;

            msg = cserver->get_msg(40);
            if ( !msg ) { // the network went down?
                unlink( tmp_file.c_str());
                throw ( 19 );
            }

            if ( msg->type == M_END )
                break;

            if ( msg->type != M_FILE_CHUNK ) {
                unlink( tmp_file.c_str());
                delete msg;
                throw ( 20 );
            }

            FileChunkMsg *fcmsg = dynamic_cast<FileChunkMsg*>( msg );
            if ( write( obj_fd, fcmsg->buffer, fcmsg->len ) != ( ssize_t )fcmsg->len ) {
                unlink( tmp_file.c_str());
                delete msg;
                throw ( 21 );
            }
        }

        delete msg;
        if( close( obj_fd ) == 0 )
            rename( tmp_file.c_str(), job.outputFile().c_str());
        else
            unlink( tmp_file.c_str());
    }

    } catch ( int x ) {
        delete cserver;
        cserver = 0;
        throw( x );
    }
    delete cserver;
    return status;
}

string md5_for_file( const string & file )
{
    md5_state_t state;
    string result;

    md5_init(&state);
    FILE *f = fopen( file.c_str(), "rb" );
    if ( !f )
        return result;

    md5_byte_t buffer[40000];

    while ( true ) {
        size_t size = fread(buffer, 1, 40000, f);
        if ( !size )
            break;
        md5_append(&state, buffer, size );
    }

    md5_byte_t digest[16];
    md5_finish(&state, digest);

    char digest_cache[33];
    for (int di = 0; di < 16; ++di)
        sprintf(digest_cache + di * 2, "%02x", digest[di]);
    digest_cache[32] = 0;
    result = digest_cache;
    return result;
}

static bool
maybe_build_local (MsgChannel *scheduler, UseCSMsg *usecs, CompileJob &job,
		   int &ret)
{
    sockaddr_in name;
    socklen_t len = sizeof(name);
    int error = getsockname(scheduler->fd, (struct sockaddr*)&name, &len);
    if ( !error ) {
	if ( usecs->hostname == inet_ntoa( name.sin_addr ) ) {
	    trace() << "building myself, but telling localhost\n";
	    unsigned int port = usecs->port;
	    int job_id = usecs->job_id;
	    job.setJobID( job_id );
	    job.setEnvironmentVersion( "__client" );
	    MsgChannel *cserver = Service::createChannel( "127.0.0.1", port, 0 ); // 0 == no time out
	    if ( !cserver ) // very unlikely as we talked before with him
		throw ( 2 );
	    CompileFileMsg compile_file( &job );
	    if ( !cserver->send_msg( compile_file ) ) {
		log_info() << "write of job failed" << endl;
		delete cserver;
		cserver = 0;
		throw( 9 );
	    }
            struct timeval begintv,  endtv;
            struct rusage ru;

            gettimeofday(&begintv, 0 );
	    ret = build_local( job, &ru );
            gettimeofday(&endtv, 0 );

            // filling the stats, so the daemon can play proxy for us
            JobDoneMsg msg( job_id, ret );

            msg.real_msec = ( endtv.tv_sec - begintv.tv_sec ) * 1000 + ( endtv.tv_usec - begintv.tv_usec ) / 1000;
            struct stat st;
            if ( !stat( job.outputFile().c_str(), &st ) )
                msg.out_uncompressed = st.st_size;
            msg.user_msec = ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000;
            msg.sys_msec = ru.ru_stime.tv_sec * 1000 + ru.ru_stime.tv_usec / 1000;
            msg.pfaults = ru.ru_majflt + ru.ru_minflt + ru.ru_nswap ;
            msg.exitcode = ret;

            if ( IS_PROTOCOL_16( cserver ) )
                cserver->send_msg( msg );
            else
                cserver->send_msg( EndMsg() );
	    delete cserver;
	    return true;
	}
    }
    return false;
}

int build_remote(CompileJob &job, MsgChannel *scheduler, const Environments &_envs, int permill )
{
    srand( time( 0 ) + getpid() );

    int torepeat = 1;

    // older compilers do not support the options we need to make it reproducable
#if defined(__GNUC__) && ( ( (__GNUC__ == 3) && (__GNUC_MINOR__ >= 3) ) || (__GNUC__ >=4) )
    if ( rand() % 1000 < permill)
        torepeat = 3;
#endif

    trace() << job.inputFile() << " compiled " << torepeat << " times on " << job.targetPlatform() << "\n";

    map<string, string> versionfile_map, version_map;
    Environments envs = rip_out_paths( _envs, version_map, versionfile_map );
    if (!envs.size()) {
	log_error() << "$ICECC_VERSION needs to point to .tar.bz2 files\n";
	throw(22);
    }

    if ( torepeat == 1 ) {
        string fake_filename;
/*        list<string> args = job.remoteFlags();
        for ( list<string>::const_iterator it = args.begin(); it != args.end(); ++it )
            fake_filename += "/" + *it;
        args = job.restFlags();
        for ( list<string>::const_iterator it = args.begin(); it != args.end(); ++it )
            fake_filename += "/" + *it;
*/
        fake_filename += get_absfilename( job.inputFile() );
        GetCSMsg getcs (envs, fake_filename, job.language(), torepeat, job.targetPlatform(), job.argumentFlags() );
        if (!scheduler->send_msg (getcs)) {
            log_warning() << "asked for CS\n";
            throw( 0 );
        }

        UseCSMsg *usecs = get_server( scheduler );
	int ret;
	if (!maybe_build_local (scheduler, usecs, job, ret))
            ret = build_remote_int( job, usecs,
	    			    version_map[usecs->host_platform],
				    versionfile_map[usecs->host_platform],
				    0, true );
        delete usecs;
        return ret;
    } else {
        char preproc[PATH_MAX];
        dcc_make_tmpnam( "icecc", ".ix", preproc, 0 );
        int cpp_fd = open(preproc, O_WRONLY );
	/* When call_cpp returns normally (for the parent) it will have closed
	   the write fd, i.e. cpp_fd.  */
        pid_t cpp_pid = call_cpp(job, cpp_fd );
        if ( cpp_pid == -1 ) {
            ::unlink( preproc );
            throw( 10 );
        }
        int status = 255;
        waitpid( cpp_pid, &status, 0);
        if ( status ) { // failure
            ::unlink( preproc );
            return WEXITSTATUS( status );
        }


        char rand_seed[400]; // "designed to be oversized" (Levi's)
        sprintf( rand_seed, "-frandom-seed=%d", rand() );
        job.appendFlag( rand_seed, Arg_Remote );

        GetCSMsg getcs (envs, get_absfilename( job.inputFile() ), job.language(), torepeat, job.targetPlatform(), job.argumentFlags() );
        if (!scheduler->send_msg (getcs)) {
            log_warning() << "asked for CS\n";
            throw( 0 );
        }

        map<pid_t, int> jobmap;
        CompileJob *jobs = new CompileJob[torepeat];
        UseCSMsg **umsgs = new UseCSMsg*[torepeat];

        bool misc_error = false;

        for ( int i = 0; i < torepeat; i++ ) {
            jobs[i] = job;
            char buffer[PATH_MAX];
            if ( i ) {
                dcc_make_tmpnam( "icecc", ".o", buffer, 0 );
                jobs[i].setOutputFile( buffer );
            } else
                sprintf( buffer, job.outputFile().c_str() );

            umsgs[i] = get_server( scheduler );
            trace() << "got_server_for_job " << umsgs[i]->hostname << endl;

            pid_t pid = fork();
            if ( !pid ) {
                int ret = 42;
                try {
		    if (!maybe_build_local (scheduler, umsgs[i], jobs[i], ret))
			ret = build_remote_int(
				  jobs[i], umsgs[i],
				  version_map[umsgs[i]->host_platform],
                                  versionfile_map[umsgs[i]->host_platform],
				  preproc, i == 0 );
                } catch ( int error ) {
                    log_info() << "build_remote_int failed and has thrown " << error << endl;
                    kill( getpid(), SIGTERM );
                    return 0; // shouldn't matter
                }
                ::exit( ret );
                return 0; // doesn't matter
            } else {
                jobmap[pid] = i;
            }
        }
        int *exit_codes = new int[torepeat];
	for ( int i = 0; i < torepeat; i++ ) // init
		exit_codes[i] = 42;

        for ( int i = 0; i < torepeat; i++ ) {
            pid_t pid = wait( &status );
            if ( pid < 0 ) {
                log_perror( "wait failed" );
                status = -1;
            } else {
                if ( WIFSIGNALED( status ) )
                {
                    // there was some misc error in processing
                    misc_error = true;
                    break;
                }
                exit_codes[jobmap[pid]] = WEXITSTATUS( status );
            }
        }

        if (! misc_error ) {
            string first_md5 = md5_for_file( jobs[0].outputFile() );

            for ( int i = 1; i < torepeat; i++ ) {
                if ( !exit_codes[0] ) { // if the first failed, we fail anyway
                    if ( exit_codes[i] == 42 ) // they are free to fail for misc reasons
			continue;

                    if ( exit_codes[i] ) {
                        log_error() << umsgs[i]->hostname << " compiled with exit code " << exit_codes[i]
                                    << " and " << umsgs[0]->hostname << " compiled with exit code " << exit_codes[0] << " - aborting!\n";
                        ::unlink( jobs[0].outputFile().c_str());
                        exit_codes[0] = -1; // overwrite
                        break;
                    }

                    string other_md5 = md5_for_file( jobs[i].outputFile() );

                    if ( other_md5 != first_md5 ) {
                        log_error() << umsgs[i]->hostname << " compiled " << jobs[0].outputFile() << " with md5 sum " << other_md5 << "(" << jobs[i].outputFile() << ")"
                                    << " and " << umsgs[0]->hostname << " compiled with md5 sum " << first_md5 << " - aborting!\n";
                        rename( jobs[0].outputFile().c_str(), ( jobs[0].outputFile() + ".caught" ).c_str() );
                        rename( preproc, ( string( preproc ) + ".caught" ).c_str() );
                        exit_codes[0] = -1; // overwrite
                        break;
                    }
                }

                ::unlink( jobs[i].outputFile().c_str() );
                delete umsgs[i];
            }
        } else {
            ::unlink( jobs[0].outputFile().c_str() );
             for ( int i = 1; i < torepeat; i++ ) {
                 ::unlink( jobs[i].outputFile().c_str());
                 delete umsgs[i];
             }
        }

        delete umsgs[0];

        ::unlink( preproc );

        int ret = exit_codes[0];

        delete [] umsgs;
        delete [] jobs;
        delete [] exit_codes;

        if ( misc_error )
            throw ( 27 );

        return ret;
    }

    return 0;
}
