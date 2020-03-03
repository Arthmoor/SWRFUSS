/***************************************************************************
*                           STAR WARS REALITY 1.0                          *
*--------------------------------------------------------------------------*
* Star Wars Reality Code Additions and changes from the Smaug Code         *
* copyright (c) 1997 by Sean Cooper                                        *
* -------------------------------------------------------------------------*
* Starwars and Starwars Names copyright(c) Lucas Film Ltd.                 *
*--------------------------------------------------------------------------*
* SMAUG 1.0 (C) 1994, 1995, 1996 by Derek Snider                           *
* SMAUG code team: Thoric, Altrag, Blodkai, Narn, Haus,                    *
* Scryn, Rennard, Swordbearer, Gorog, Grishnakh and Tricops                *
* ------------------------------------------------------------------------ *
* Merc 2.1 Diku Mud improvments copyright (C) 1992, 1993 by Michael        *
* Chastain, Michael Quan, and Mitchell Tse.                                *
* Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,          *
* Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.     *
* ------------------------------------------------------------------------ *
*			 Low-level communication module			   *
****************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include "mud.h"
#include "md5.h"
#include "mccp.h"

/*
 * Socket and TCP/IP stuff.
 */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <netdb.h>

#define MAX_NEST	100
static OBJ_DATA *rgObjNest[MAX_NEST];


const char echo_off_str[] = { IAC, WILL, TELOPT_ECHO, '\0' };
const char echo_on_str[] = { IAC, WONT, TELOPT_ECHO, '\0' };
const char go_ahead_str[] = { IAC, GA, '\0' };

void save_sysdata args( ( SYSTEM_DATA sys ) );

/*  from act_info?  */
void show_condition( CHAR_DATA * ch, CHAR_DATA * victim );

/*
 * Global variables.
 */
DESCRIPTOR_DATA *first_descriptor;  /* First descriptor     */
DESCRIPTOR_DATA *last_descriptor;   /* Last descriptor      */
DESCRIPTOR_DATA *d_next;   /* Next descriptor in loop */
int num_descriptors;
bool mud_down; /* Shutdown       */
bool wizlock;  /* Game is wizlocked    */
time_t boot_time;
HOUR_MIN_SEC set_boot_time_struct;
HOUR_MIN_SEC *set_boot_time;
struct tm *new_boot_time;
struct tm new_boot_struct;
char str_boot_time[MAX_INPUT_LENGTH];
char lastplayercmd[MAX_INPUT_LENGTH * 2];
time_t current_time; /* Time of this pulse      */
int port;   /* Port number to be used       */
int control;   /* Controlling descriptor  */
int newdesc;   /* New descriptor    */
fd_set in_set; /* Set of desc's for reading  */
fd_set out_set;   /* Set of desc's for writing  */
fd_set exc_set;   /* Set of desc's with errors  */
int maxdesc;

/*
 * OS-dependent local functions.
 */
void game_loop args( ( void ) );
int init_socket args( ( int gport ) );
void new_descriptor args( ( int new_desc ) );
bool read_from_descriptor args( ( DESCRIPTOR_DATA * d ) );
bool write_to_descriptor( DESCRIPTOR_DATA * d, char *txt, int length );

/*
 * Other local functions (OS-independent).
 */
bool check_parse_name args( ( char *name ) );
bool check_reconnect args( ( DESCRIPTOR_DATA * d, char *name, bool fConn ) );
bool check_playing args( ( DESCRIPTOR_DATA * d, char *name, bool kick ) );
bool check_multi args( ( DESCRIPTOR_DATA * d, char *name ) );
int main args( ( int argc, char **argv ) );
void nanny args( ( DESCRIPTOR_DATA * d, char *argument ) );
bool flush_buffer args( ( DESCRIPTOR_DATA * d, bool fPrompt ) );
void read_from_buffer args( ( DESCRIPTOR_DATA * d ) );
void stop_idling args( ( CHAR_DATA * ch ) );
void free_desc args( ( DESCRIPTOR_DATA * d ) );
void display_prompt args( ( DESCRIPTOR_DATA * d ) );
void set_pager_input args( ( DESCRIPTOR_DATA * d, char *argument ) );
bool pager_output args( ( DESCRIPTOR_DATA * d ) );
void mail_count args( ( CHAR_DATA * ch ) );

int main( int argc, char **argv )
{
   struct timeval now_time;
   bool fCopyOver = FALSE;
#ifdef IMC
   int imcsocket = -1;
#endif

   /*
    * Memory debugging if needed.
    */
#if defined(MALLOC_DEBUG)
   malloc_debug( 2 );
#endif

   num_descriptors = 0;
   first_descriptor = NULL;
   last_descriptor = NULL;
   sysdata.NO_NAME_RESOLVING = TRUE;
   sysdata.WAIT_FOR_AUTH = TRUE;

   /*
    * Init time.
    */
   gettimeofday( &now_time, NULL );
   current_time = ( time_t ) now_time.tv_sec;
/*  gettimeofday( &boot_time, NULL);   okay, so it's kludgy, sue me :) */
   boot_time = time( 0 );  /*  <-- I think this is what you wanted */
   strcpy( str_boot_time, ctime( &current_time ) );

   /*
    * Init boot time.
    */
   set_boot_time = &set_boot_time_struct;
   /*
    * set_boot_time->hour   = 6;
    * set_boot_time->min    = 0;
    * set_boot_time->sec    = 0;
    */
   set_boot_time->manual = 0;

   new_boot_time = update_time( localtime( &current_time ) );
   /*
    * Copies *new_boot_time to new_boot_struct, and then points
    * new_boot_time to new_boot_struct again. -- Alty 
    */
   new_boot_struct = *new_boot_time;
   new_boot_time = &new_boot_struct;
   new_boot_time->tm_mday += 1;
   if( new_boot_time->tm_hour > 12 )
      new_boot_time->tm_mday += 1;
   new_boot_time->tm_sec = 0;
   new_boot_time->tm_min = 0;
   new_boot_time->tm_hour = 6;

   /*
    * Update new_boot_time (due to day increment) 
    */
   new_boot_time = update_time( new_boot_time );
   new_boot_struct = *new_boot_time;
   new_boot_time = &new_boot_struct;

   /*
    * Set reboot time string for do_time 
    */
   get_reboot_string(  );

   /*
    * Get the port number.
    */
   port = 4000;
   if( argc > 1 )
   {
      if( !is_number( argv[1] ) )
      {
         fprintf( stderr, "Usage: %s [port #]\n", argv[0] );
         exit( 1 );
      }
      else if( ( port = atoi( argv[1] ) ) <= 1024 )
      {
         fprintf( stderr, "Port number must be above 1024.\n" );
         exit( 1 );
      }
      if( argv[2] && argv[2][0] )
      {
         fCopyOver = TRUE;
         control = atoi( argv[3] );
#ifdef IMC
         imcsocket = atoi( argv[4] );
#endif
      }
      else
         fCopyOver = FALSE;
   }

   /*
    * Run the game.
    */
   log_string( "Booting Database" );
   boot_db( fCopyOver );
   log_string( "Initializing socket" );
   if( !fCopyOver )  /* We have already the port if copyover'ed */
      control = init_socket( port );

   sprintf( log_buf, "Star Wars Reality ready on port %d.", port );
   log_string( log_buf );

#ifdef IMC
   /*
    * Initialize and connect to IMC2 
    */
   imc_startup( FALSE, imcsocket, fCopyOver );
#endif

   if( fCopyOver )
   {
      log_string( "Initiating hotboot recovery." );
      hotboot_recover(  );
   }
   game_loop(  );
   close( control );
#ifdef IMC
   imc_shutdown( FALSE );
#endif
   /*
    * That's all, folks.
    */
   log_string( "Normal termination of game." );
   exit( 0 );
   return 0;
}

int init_socket( int gport )
{
   char hostname[64];
   struct sockaddr_in sa;
   struct hostent *hp;
   struct servent *sp;
   int x = 1;
   int fd;

   gethostname( hostname, sizeof( hostname ) );


   if( ( fd = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
   {
      perror( "Init_socket: socket" );
      exit( 1 );
   }

   if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, ( void * )&x, sizeof( x ) ) < 0 )
   {
      perror( "Init_socket: SO_REUSEADDR" );
      close( fd );
      exit( 1 );
   }

#if defined(SO_DONTLINGER) && !defined(SYSV)
   {
      struct linger ld;

      ld.l_onoff = 1;
      ld.l_linger = 1000;

      if( setsockopt( fd, SOL_SOCKET, SO_DONTLINGER, ( void * )&ld, sizeof( ld ) ) < 0 )
      {
         perror( "Init_socket: SO_DONTLINGER" );
         close( fd );
         exit( 1 );
      }
   }
#endif

   hp = gethostbyname( hostname );
   sp = getservbyname( "service", "mud" );
   memset( &sa, '\0', sizeof( sa ) );
   sa.sin_family = AF_INET;   /* hp->h_addrtype; */
   sa.sin_port = htons( gport );

   if( bind( fd, ( struct sockaddr * )&sa, sizeof( sa ) ) == -1 )
   {
      perror( "Init_socket: bind" );
      close( fd );
      exit( 1 );
   }

   if( listen( fd, 50 ) < 0 )
   {
      perror( "Init_socket: listen" );
      close( fd );
      exit( 1 );
   }

   return fd;
}

/*
static void SegVio()
{
  CHAR_DATA *ch;
  char buf[MAX_STRING_LENGTH];

  log_string( "SEGMENTATION VIOLATION" );
  log_string( lastplayercmd );
  for ( ch = first_char; ch; ch = ch->next )
  {
    sprintf( buf, "%cPC: %-20s room: %d", IS_NPC(ch) ? 'N' : ' ',
    		ch->name, ch->in_room->vnum );
    log_string( buf );  
  }
  exit(0);
}
*/

/*
 * LAG alarm!							-Thoric
 */
static void caught_alarm( int signum )
{
   char buf[MAX_STRING_LENGTH];
   bug( "ALARM CLOCK!" );
   strcpy( buf, "Alas, the hideous malevalent entity known only as 'Lag' rises once more!\n\r" );
   echo_to_all( AT_IMMORT, buf, ECHOTAR_ALL );
   if( newdesc )
   {
      FD_CLR( newdesc, &in_set );
      FD_CLR( newdesc, &out_set );
      log_string( "clearing newdesc" );
   }
   game_loop(  );
   close( control );

   log_string( "Normal termination of game." );
   exit( 0 );
}

bool check_bad_desc( int desc )
{
   if( FD_ISSET( desc, &exc_set ) )
   {
      FD_CLR( desc, &in_set );
      FD_CLR( desc, &out_set );
      log_string( "Bad FD caught and disposed." );
      return TRUE;
   }
   return FALSE;
}


void accept_new( int ctrl )
{
   static struct timeval null_time;
   DESCRIPTOR_DATA *d;
   /*
    * int maxdesc; Moved up for use with id.c as extern 
    */

#if defined(MALLOC_DEBUG)
   if( malloc_verify(  ) != 1 )
      abort(  );
#endif

   /*
    * Poll all active descriptors.
    */
   FD_ZERO( &in_set );
   FD_ZERO( &out_set );
   FD_ZERO( &exc_set );
   FD_SET( ctrl, &in_set );
   maxdesc = ctrl;
   newdesc = 0;
   for( d = first_descriptor; d; d = d->next )
   {
      maxdesc = UMAX( maxdesc, d->descriptor );
      FD_SET( d->descriptor, &in_set );
      FD_SET( d->descriptor, &out_set );
      FD_SET( d->descriptor, &exc_set );
      if( d == last_descriptor )
         break;
   }

   if( select( maxdesc + 1, &in_set, &out_set, &exc_set, &null_time ) < 0 )
   {
      perror( "accept_new: select: poll" );
      exit( 1 );
   }

   if( FD_ISSET( ctrl, &exc_set ) )
   {
      bug( "Exception raise on controlling descriptor %d", ctrl );
      FD_CLR( ctrl, &in_set );
      FD_CLR( ctrl, &out_set );
   }
   else if( FD_ISSET( ctrl, &in_set ) )
   {
      newdesc = ctrl;
      new_descriptor( newdesc );
   }
}

void game_loop( void )
{
   struct timeval last_time;
   char cmdline[MAX_INPUT_LENGTH];
   DESCRIPTOR_DATA *d;
/*  time_t	last_check = 0;  */

   signal( SIGPIPE, SIG_IGN );
   signal( SIGALRM, caught_alarm );
   /*
    * signal( SIGSEGV, SegVio ); 
    */
   gettimeofday( &last_time, NULL );
   current_time = ( time_t ) last_time.tv_sec;

   /*
    * Main loop 
    */
   while( !mud_down )
   {
      accept_new( control );
      /*
       * Kick out descriptors with raised exceptions
       * or have been idle, then check for input.
       */
      for( d = first_descriptor; d; d = d_next )
      {
         if( d == d->next )
         {
            bug( "descriptor_loop: loop found & fixed" );
            d->next = NULL;
         }
         d_next = d->next;

         d->idle++;  /* make it so a descriptor can idle out */
         if( FD_ISSET( d->descriptor, &exc_set ) )
         {
            FD_CLR( d->descriptor, &in_set );
            FD_CLR( d->descriptor, &out_set );
            if( d->character && ( d->connected == CON_PLAYING || d->connected == CON_EDITING ) )
               save_char_obj( d->character );
            d->outtop = 0;
            close_socket( d, TRUE );
            continue;
         }
         else if( ( !d->character && d->idle > 360 )  /* 2 mins */
                  || ( d->connected != CON_PLAYING && d->idle > 1200 )  /* 5 mins */
                  || d->idle > 28800 ) /* 2 hrs  */
         {
            write_to_descriptor( d, "Idle timeout... disconnecting.\n\r", 0 );
            d->outtop = 0;
            close_socket( d, TRUE );
            continue;
         }
         else
         {
            d->fcommand = FALSE;

            if( FD_ISSET( d->descriptor, &in_set ) )
            {
               d->idle = 0;
               if( d->character )
                  d->character->timer = 0;
               if( !read_from_descriptor( d ) )
               {
                  FD_CLR( d->descriptor, &out_set );
                  if( d->character && ( d->connected == CON_PLAYING || d->connected == CON_EDITING ) )
                     save_char_obj( d->character );
                  d->outtop = 0;
                  close_socket( d, FALSE );
                  continue;
               }
            }

            if( d->character && d->character->wait > 0 )
            {
               --d->character->wait;
               continue;
            }

            read_from_buffer( d );
            if( d->incomm[0] != '\0' )
            {
               d->fcommand = TRUE;
               stop_idling( d->character );

               strcpy( cmdline, d->incomm );
               d->incomm[0] = '\0';

               if( d->character )
                  set_cur_char( d->character );

               if( d->pagepoint )
                  set_pager_input( d, cmdline );
               else
                  switch ( d->connected )
                  {
                     default:
                        nanny( d, cmdline );
                        break;
                     case CON_PLAYING:
                        interpret( d->character, cmdline );
                        break;
                     case CON_EDITING:
                        edit_buffer( d->character, cmdline );
                        break;
                  }
            }
         }
         if( d == last_descriptor )
            break;
      }

#ifdef IMC
      imc_loop(  );
#endif

      /*
       * Autonomous game motion.
       */
      update_handler(  );

      /*
       * Output.
       */
      for( d = first_descriptor; d; d = d_next )
      {
         d_next = d->next;

         if( ( d->fcommand || d->outtop > 0 ) && FD_ISSET( d->descriptor, &out_set ) )
         {
            if( d->pagepoint )
            {
               if( !pager_output( d ) )
               {
                  if( d->character && ( d->connected == CON_PLAYING || d->connected == CON_EDITING ) )
                     save_char_obj( d->character );
                  d->outtop = 0;
                  close_socket( d, FALSE );
               }
            }
            else if( !flush_buffer( d, TRUE ) )
            {
               if( d->character && ( d->connected == CON_PLAYING || d->connected == CON_EDITING ) )
                  save_char_obj( d->character );
               d->outtop = 0;
               close_socket( d, FALSE );
            }
         }
         if( d == last_descriptor )
            break;
      }

      /*
       * Synchronize to a clock.
       * Sleep( last_time + 1/PULSE_PER_SECOND - now ).
       * Careful here of signed versus unsigned arithmetic.
       */
      {
         struct timeval now_time;
         long secDelta;
         long usecDelta;

         gettimeofday( &now_time, NULL );
         usecDelta = ( ( int )last_time.tv_usec ) - ( ( int )now_time.tv_usec ) + 1000000 / PULSE_PER_SECOND;
         secDelta = ( ( int )last_time.tv_sec ) - ( ( int )now_time.tv_sec );
         while( usecDelta < 0 )
         {
            usecDelta += 1000000;
            secDelta -= 1;
         }

         while( usecDelta >= 1000000 )
         {
            usecDelta -= 1000000;
            secDelta += 1;
         }

         if( secDelta > 0 || ( secDelta == 0 && usecDelta > 0 ) )
         {
            struct timeval stall_time;

            stall_time.tv_usec = usecDelta;
            stall_time.tv_sec = secDelta;
            if( select( 0, NULL, NULL, NULL, &stall_time ) < 0 )
            {
               perror( "game_loop: select: stall" );
               exit( 1 );
            }
         }
      }

      gettimeofday( &last_time, NULL );
      current_time = ( time_t ) last_time.tv_sec;

      /*
       * Check every 5 seconds...  (don't need it right now)
       * if ( last_check+5 < current_time )
       * {
       * CHECK_LINKS(first_descriptor, last_descriptor, next, prev,
       * DESCRIPTOR_DATA);
       * last_check = current_time;
       * }
       */
   }
   return;
}


void new_descriptor( int new_desc )
{
   char buf[MAX_STRING_LENGTH];
   DESCRIPTOR_DATA *dnew;
   BAN_DATA *pban;
   struct hostent *from;
   char *hostname;
   struct sockaddr_in sock;
   size_t desc, size;

   set_alarm( 20 );
   size = sizeof( sock );
   if( check_bad_desc( new_desc ) )
   {
      set_alarm( 0 );
      return;
   }
   set_alarm( 20 );
   if( ( desc = accept( new_desc, ( struct sockaddr * )&sock, &size ) ) < 0 )
   {
      perror( "New_descriptor: accept" );
/*	sprintf(bugbuf, "[*****] BUG: New_descriptor: accept");
	log_string_plus( bugbuf, LOG_COMM, sysdata.log_level ); */
      set_alarm( 0 );
      return;
   }
   if( check_bad_desc( new_desc ) )
   {
      set_alarm( 0 );
      return;
   }
#if !defined(FNDELAY)
#define FNDELAY O_NDELAY
#endif

   set_alarm( 20 );
   if( fcntl( desc, F_SETFL, FNDELAY ) == -1 )
   {
      perror( "New_descriptor: fcntl: FNDELAY" );
      set_alarm( 0 );
      return;
   }
   if( check_bad_desc( new_desc ) )
      return;

   CREATE( dnew, DESCRIPTOR_DATA, 1 );
   dnew->next = NULL;
   dnew->descriptor = desc;
   dnew->connected = CON_GET_NAME;
   dnew->outsize = 2000;
   dnew->idle = 0;
   dnew->lines = 0;
   dnew->scrlen = 24;
   dnew->port = ntohs( sock.sin_port );
   dnew->newstate = 0;
   dnew->prevcolor = 0x07;
   dnew->can_compress = FALSE;
   CREATE( dnew->mccp, MCCP, 1 );
   dnew->original = NULL;
   dnew->character = NULL;

   CREATE( dnew->outbuf, char, dnew->outsize );

   strcpy( buf, inet_ntoa( sock.sin_addr ) );
   sprintf( log_buf, "Sock.sinaddr:  %s, port %hd.", buf, dnew->port );
   log_string_plus( log_buf, LOG_COMM, sysdata.log_level );

   dnew->host = STRALLOC( buf );

   from = gethostbyaddr( ( char * )&sock.sin_addr, sizeof( sock.sin_addr ), AF_INET );
   hostname = STRALLOC( ( char * )( from ? from->h_name : "" ) );

   for( pban = first_ban; pban; pban = pban->next )
   {
      if( ( !str_prefix( pban->name, dnew->host ) || !str_suffix( pban->name, hostname ) ) && pban->level >= LEVEL_SUPREME )
      {
         write_to_descriptor( dnew, "Your site has been banned from this Mud.\n\r", 0 );
         free_desc( dnew );
         set_alarm( 0 );
         return;
      }
   }

   if( !sysdata.NO_NAME_RESOLVING )
   {
      STRFREE( dnew->host );
      dnew->host = STRALLOC( ( char * )( from ? from->h_name : buf ) );
   }

   /*
    * Init descriptor data.
    */
   if( !last_descriptor && first_descriptor )
   {
      DESCRIPTOR_DATA *d;

      bug( "New_descriptor: last_desc is NULL, but first_desc is not! ...fixing" );
      for( d = first_descriptor; d; d = d->next )
         if( !d->next )
            last_descriptor = d;
   }

   LINK( dnew, first_descriptor, last_descriptor, next, prev );

   /*
    * MCCP Compression 
    */
   write_to_buffer( dnew, will_compress2_str, 0 );

   /*
    * Send the greeting.
    */
   {
      extern char *help_greeting;
      if( help_greeting[0] == '.' )
         write_to_buffer( dnew, help_greeting + 1, 0 );
      else
         write_to_buffer( dnew, help_greeting, 0 );
   }

   if( ++num_descriptors > sysdata.maxplayers )
      sysdata.maxplayers = num_descriptors;
   if( sysdata.maxplayers > sysdata.alltimemax )
   {
      if( sysdata.time_of_max )
         DISPOSE( sysdata.time_of_max );
      sprintf( buf, "%24.24s", ctime( &current_time ) );
      sysdata.time_of_max = str_dup( buf );
      sysdata.alltimemax = sysdata.maxplayers;
      sprintf( log_buf, "Broke all-time maximum player record: %d", sysdata.alltimemax );
      log_string_plus( log_buf, LOG_COMM, sysdata.log_level );
      to_channel( log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL );
      save_sysdata( sysdata );
   }
   set_alarm( 0 );
   return;
}

void free_desc( DESCRIPTOR_DATA * d )
{
   close( d->descriptor );
   STRFREE( d->host );
   DISPOSE( d->outbuf );
   if( d->pagebuf )
      DISPOSE( d->pagebuf );
   compressEnd( d );
   DISPOSE( d->mccp );
   DISPOSE( d );
   --num_descriptors;
   return;
}

void close_socket( DESCRIPTOR_DATA * dclose, bool force )
{
   CHAR_DATA *ch;
   DESCRIPTOR_DATA *d;
   bool DoNotUnlink = FALSE;

   /*
    * flush outbuf 
    */
   if( !force && dclose->outtop > 0 )
      flush_buffer( dclose, FALSE );

   /*
    * say bye to whoever's snooping this descriptor 
    */
   if( dclose->snoop_by )
      write_to_buffer( dclose->snoop_by, "Your victim has left the game.\n\r", 0 );

   /*
    * stop snooping everyone else 
    */
   for( d = first_descriptor; d; d = d->next )
      if( d->snoop_by == dclose )
         d->snoop_by = NULL;

   /*
    * Check for switched people who go link-dead. -- Altrag 
    */
   if( dclose->original )
   {
      if( ( ch = dclose->character ) != NULL )
         do_return( ch, "" );
      else
      {
         bug( "Close_socket: dclose->original without character %s",
              ( dclose->original->name ? dclose->original->name : "unknown" ) );
         dclose->character = dclose->original;
         dclose->original = NULL;
      }
   }

   ch = dclose->character;

   /*
    * sanity check :( 
    */
   if( !dclose->prev && dclose != first_descriptor )
   {
      DESCRIPTOR_DATA *dp, *dn;
      bug( "Close_socket: %s desc:%p != first_desc:%p and desc->prev = NULL!",
           ch ? ch->name : d->host, dclose, first_descriptor );
      dp = NULL;
      for( d = first_descriptor; d; d = dn )
      {
         dn = d->next;
         if( d == dclose )
         {
            bug( "Close_socket: %s desc:%p found, prev should be:%p, fixing.", ch ? ch->name : d->host, dclose, dp );
            dclose->prev = dp;
            break;
         }
         dp = d;
      }
      if( !dclose->prev )
      {
         bug( "Close_socket: %s desc:%p could not be found!.", ch ? ch->name : dclose->host, dclose );
         DoNotUnlink = TRUE;
      }
   }
   if( !dclose->next && dclose != last_descriptor )
   {
      DESCRIPTOR_DATA *dp, *dn;
      bug( "Close_socket: %s desc:%p != last_desc:%p and desc->next = NULL!",
           ch ? ch->name : d->host, dclose, last_descriptor );
      dn = NULL;
      for( d = last_descriptor; d; d = dp )
      {
         dp = d->prev;
         if( d == dclose )
         {
            bug( "Close_socket: %s desc:%p found, next should be:%p, fixing.", ch ? ch->name : d->host, dclose, dn );
            dclose->next = dn;
            break;
         }
         dn = d;
      }
      if( !dclose->next )
      {
         bug( "Close_socket: %s desc:%p could not be found!.", ch ? ch->name : dclose->host, dclose );
         DoNotUnlink = TRUE;
      }
   }

   if( dclose->character )
   {
      sprintf( log_buf, "Closing link to %s.", ch->name );
      log_string_plus( log_buf, LOG_COMM, UMAX( sysdata.log_level, ch->top_level ) );
/*
	if ( ch->top_level < LEVEL_DEMI )
	  to_channel( log_buf, CHANNEL_MONITOR, "Monitor", ch->top_level );
*/
      if( dclose->connected == CON_PLAYING || dclose->connected == CON_EDITING )
      {
         act( AT_ACTION, "$n has lost $s link.", ch, NULL, NULL, TO_ROOM );
         ch->desc = NULL;
      }
      else
      {
         /*
          * clear descriptor pointer to get rid of bug message in log 
          */
         dclose->character->desc = NULL;
         free_char( dclose->character );
      }
   }


   if( !DoNotUnlink )
   {
      /*
       * make sure loop doesn't get messed up 
       */
      if( d_next == dclose )
         d_next = d_next->next;
      UNLINK( dclose, first_descriptor, last_descriptor, next, prev );
   }

   compressEnd( dclose );

   if( dclose->descriptor == maxdesc )
      --maxdesc;

   free_desc( dclose );
   return;
}

bool read_from_descriptor( DESCRIPTOR_DATA * d )
{
   int iStart;

   /*
    * Hold horses if pending command already. 
    */
   if( d->incomm[0] != '\0' )
      return TRUE;

   /*
    * Check for overflow. 
    */
   iStart = strlen( d->inbuf );
   if( iStart >= sizeof( d->inbuf ) - 10 )
   {
      sprintf( log_buf, "%s input overflow!", d->host );
      log_string( log_buf );
      write_to_descriptor( d, "\n\r*** PUT A LID ON IT!!! ***\n\r", 0 );
      return FALSE;
   }

   for( ;; )
   {
      int nRead;

      nRead = read( d->descriptor, d->inbuf + iStart, sizeof( d->inbuf ) - 10 - iStart );
      if( nRead > 0 )
      {
         iStart += nRead;
         if( d->inbuf[iStart - 1] == '\n' || d->inbuf[iStart - 1] == '\r' )
            break;
      }
      else if( nRead == 0 )
      {
         log_string_plus( "EOF encountered on read.", LOG_COMM, sysdata.log_level );
         return FALSE;
      }
      else if( errno == EWOULDBLOCK )
         break;
      else
      {
         perror( "Read_from_descriptor" );
         return FALSE;
      }
   }

   d->inbuf[iStart] = '\0';
   return TRUE;
}



/*
 * Transfer one line from input buffer to input line.
 */
void read_from_buffer( DESCRIPTOR_DATA * d )
{
   int i, j, k, iac = 0;

   /*
    * Hold horses if pending command already.
    */
   if( d->incomm[0] != '\0' )
      return;

   /*
    * Look for at least one new line.
    */
   for( i = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r' && i < MAX_INBUF_SIZE; i++ )
   {
      if( d->inbuf[i] == '\0' )
         return;
   }

   /*
    * Canonical input processing.
    */
   for( i = 0, k = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++ )
   {
      if( k >= 254 )
      {
         write_to_descriptor( d, "Line too long.\n\r", 0 );

         /*
          * skip the rest of the line 
          */
         /*
          * for ( ; d->inbuf[i] != '\0' || i>= MAX_INBUF_SIZE ; i++ )
          * {
          * if ( d->inbuf[i] == '\n' || d->inbuf[i] == '\r' )
          * break;
          * }
          */
         d->inbuf[i] = '\n';
         d->inbuf[i + 1] = '\0';
         break;
      }

      if( d->inbuf[i] == ( signed char )IAC )
         iac = 1;
      else if( iac == 1
               && ( d->inbuf[i] == ( signed char )DO || d->inbuf[i] == ( signed char )DONT
                    || d->inbuf[i] == ( signed char )WILL ) )
         iac = 2;
      else if( iac == 2 )
      {
         iac = 0;
         if( d->inbuf[i] == ( signed char )TELOPT_COMPRESS2 )
         {
            if( d->inbuf[i - 1] == ( signed char )DO )
               compressStart( d );
            else if( d->inbuf[i - 1] == ( signed char )DONT )
               compressEnd( d );
         }
      }
      else if( d->inbuf[i] == '\b' && k > 0 )
         --k;
      else if( isascii( d->inbuf[i] ) && isprint( d->inbuf[i] ) )
         d->incomm[k++] = d->inbuf[i];
   }

   /*
    * Finish off the line.
    */
   if( k == 0 )
      d->incomm[k++] = ' ';
   d->incomm[k] = '\0';

   /*
    * Deal with bozos with #repeat 1000 ...
    */
   if( k > 1 || d->incomm[0] == '!' )
   {
      if( d->incomm[0] != '!' && strcmp( d->incomm, d->inlast ) )
      {
         d->repeat = 0;
      }
      else
      {
         if( ++d->repeat >= 20 )
         {
/*		sprintf( log_buf, "%s input spamming!", d->host );
		log_string( log_buf );
*/
            write_to_descriptor( d, "\n\r*** PUT A LID ON IT!!! ***\n\r", 0 );
         }
      }
   }

   /*
    * Do '!' substitution.
    */
   if( d->incomm[0] == '!' )
      strcpy( d->incomm, d->inlast );
   else
      strcpy( d->inlast, d->incomm );

   /*
    * Shift the input buffer.
    */
   while( d->inbuf[i] == '\n' || d->inbuf[i] == '\r' )
      i++;
   for( j = 0; ( d->inbuf[j] = d->inbuf[i + j] ) != '\0'; j++ )
      ;
   return;
}



/*
 * Low level output function.
 */
bool flush_buffer( DESCRIPTOR_DATA * d, bool fPrompt )
{
   char buf[MAX_INPUT_LENGTH];
   CHAR_DATA *ch;

   ch = d->original ? d->original : d->character;
   if( ch && ch->fighting && ch->fighting->who )
      show_condition( ch, ch->fighting->who );

   /*
    * If buffer has more than 4K inside, spit out .5K at a time   -Thoric
    */
   if( !mud_down && d->outtop > 4096 )
   {
      memcpy( buf, d->outbuf, 512 );
      memmove( d->outbuf, d->outbuf + 512, d->outtop - 512 );
      d->outtop -= 512;
      if( d->snoop_by )
      {
         char snoopbuf[MAX_INPUT_LENGTH];

         buf[512] = '\0';
         if( d->character && d->character->name )
         {
            if( d->original && d->original->name )
               sprintf( snoopbuf, "%s (%s)", d->character->name, d->original->name );
            else
               sprintf( snoopbuf, "%s", d->character->name );
            write_to_buffer( d->snoop_by, snoopbuf, 0 );
         }
         write_to_buffer( d->snoop_by, "% ", 2 );
         write_to_buffer( d->snoop_by, buf, 0 );
      }
      if( !write_to_descriptor( d, buf, 512 ) )
      {
         d->outtop = 0;
         return FALSE;
      }
      return TRUE;
   }


   /*
    * Bust a prompt.
    */
   if( fPrompt && !mud_down && d->connected == CON_PLAYING )
   {
      ch = d->original ? d->original : d->character;
      if( IS_SET( ch->act, PLR_BLANK ) )
         write_to_buffer( d, "\n\r", 2 );

      if( IS_SET( ch->act, PLR_PROMPT ) )
         display_prompt( d );
      if( IS_SET( ch->act, PLR_TELNET_GA ) )
         write_to_buffer( d, go_ahead_str, 0 );
   }

   /*
    * Short-circuit if nothing to write.
    */
   if( d->outtop == 0 )
      return TRUE;

   /*
    * Snoop-o-rama.
    */
   if( d->snoop_by )
   {
      /*
       * without check, 'force mortal quit' while snooped caused crash, -h 
       */
      if( d->character && d->character->name )
      {
         /*
          * Show original snooped names. -- Altrag 
          */
         if( d->original && d->original->name )
            sprintf( buf, "%s (%s)", d->character->name, d->original->name );
         else
            sprintf( buf, "%s", d->character->name );
         write_to_buffer( d->snoop_by, buf, 0 );
      }
      write_to_buffer( d->snoop_by, "% ", 2 );
      write_to_buffer( d->snoop_by, d->outbuf, d->outtop );
   }

   /*
    * OS-dependent output.
    */
   if( !write_to_descriptor( d, d->outbuf, d->outtop ) )
   {
      d->outtop = 0;
      return FALSE;
   }
   else
   {
      d->outtop = 0;
      return TRUE;
   }
}



/*
 * Append onto an output buffer.
 */
void write_to_buffer( DESCRIPTOR_DATA * d, const char *txt, int length )
{
   if( !d )
   {
      bug( "Write_to_buffer: NULL descriptor" );
      return;
   }

   /*
    * Normally a bug... but can happen if loadup is used.
    */
   if( !d->outbuf )
      return;

   /*
    * Find length in case caller didn't.
    */
   if( length <= 0 )
      length = strlen( txt );

/* Uncomment if debugging or something
    if ( length != strlen(txt) )
    {
	bug( "Write_to_buffer: length(%d) != strlen(txt)!", length );
	length = strlen(txt);
    }
*/

   /*
    * Initial \n\r if needed.
    */
   if( d->outtop == 0 && !d->fcommand )
   {
      d->outbuf[0] = '\n';
      d->outbuf[1] = '\r';
      d->outtop = 2;
   }

   /*
    * Expand the buffer as needed.
    */
   while( d->outtop + length >= d->outsize )
   {
      if( d->outsize > 32000 )
      {
         /*
          * empty buffer 
          */
         d->outtop = 0;
         bug( "Buffer overflow. Closing (%s).", d->character ? d->character->name : "???" );
         close_socket( d, TRUE );
         return;
      }
      d->outsize *= 2;
      RECREATE( d->outbuf, char, d->outsize );
   }

   /*
    * Copy.
    */
   strncpy( d->outbuf + d->outtop, txt, length );
   d->outtop += length;
   d->outbuf[d->outtop] = '\0';
   return;
}


/*
* This is the MCCP version. Use write_to_descriptor_old to send non-compressed
* text.
* Updated to run with the block checks by Orion... if it doesn't work, blame
* him.;P -Orion
*/
bool write_to_descriptor( DESCRIPTOR_DATA * d, char *txt, int length )
{
   int iStart = 0;
   int nWrite = 0;
   int nBlock;
   int iErr;
   int len;

   if( length <= 0 )
      length = strlen( txt );

   if( d && d->mccp->out_compress )
   {
      d->mccp->out_compress->next_in = ( unsigned char * )txt;
      d->mccp->out_compress->avail_in = length;

      while( d->mccp->out_compress->avail_in )
      {
         d->mccp->out_compress->avail_out =
            COMPRESS_BUF_SIZE - ( d->mccp->out_compress->next_out - d->mccp->out_compress_buf );

         if( d->mccp->out_compress->avail_out )
         {
            int status = deflate( d->mccp->out_compress, Z_SYNC_FLUSH );

            if( status != Z_OK )
               return FALSE;
         }

         len = d->mccp->out_compress->next_out - d->mccp->out_compress_buf;
         if( len > 0 )
         {
            for( iStart = 0; iStart < len; iStart += nWrite )
            {
               nBlock = UMIN( len - iStart, 4096 );
               nWrite = send( d->descriptor, d->mccp->out_compress_buf + iStart, nBlock, 0 );
               if( nWrite == -1 )
               {
                  iErr = errno;
                  if( iErr == EWOULDBLOCK )
                  {
                     /*
                      * This is a SPAMMY little bug error. I would suggest
                      * not using it, but I've included it in case. -Orion
                      *
                      perror( "Write_to_descriptor: Send is blocking" );
                      */
                     nWrite = 0;
                     continue;
                  }
                  else
                  {
                     perror( "Write_to_descriptor" );
                     return FALSE;
                  }
               }

               if( !nWrite )
                  break;
            }

            if( !iStart )
               break;

            if( iStart < len )
               memmove( d->mccp->out_compress_buf, d->mccp->out_compress_buf + iStart, len - iStart );

            d->mccp->out_compress->next_out = d->mccp->out_compress_buf + len - iStart;
         }
      }
      return TRUE;
   }

   for( iStart = 0; iStart < length; iStart += nWrite )
   {
      nBlock = UMIN( length - iStart, 4096 );
      nWrite = send( d->descriptor, txt + iStart, nBlock, 0 );
      if( nWrite == -1 )
      {
         iErr = errno;
         if( iErr == EWOULDBLOCK )
         {
            /*
             * This is a SPAMMY little bug error. I would suggest
             * not using it, but I've included it in case. -Orion
             *
             perror( "Write_to_descriptor: Send is blocking" );
             */
            nWrite = 0;
            continue;
         }
         else
         {
            perror( "Write_to_descriptor" );
            return FALSE;
         }
      }
   }
   return TRUE;
}

/*
 *
 * Added block checking to prevent random booting of the descriptor. Thanks go
 * out to Rustry for his suggestions. -Orion
 */
bool write_to_descriptor_old( int desc, char *txt, int length )
{
   int iStart = 0;
   int nWrite = 0;
   int nBlock = 0;
   int iErr = 0;

   if( length <= 0 )
      length = strlen( txt );

   for( iStart = 0; iStart < length; iStart += nWrite )
   {
      nBlock = UMIN( length - iStart, 4096 );
      nWrite = send( desc, txt + iStart, nBlock, 0 );

      if( nWrite == -1 )
      {
         iErr = errno;
         if( iErr == EWOULDBLOCK )
         {
            /*
             * This is a SPAMMY little bug error. I would suggest
             * not using it, but I've included it in case. -Orion
             *
             perror( "Write_to_descriptor: Send is blocking" );
             */
            nWrite = 0;
            continue;
         }
         else
         {
            perror( "Write_to_descriptor" );
            return FALSE;
         }
      }
   }
   return TRUE;
}

void show_title( DESCRIPTOR_DATA * d )
{
   CHAR_DATA *ch;

   ch = d->character;

   if( !IS_SET( ch->pcdata->flags, PCFLAG_NOINTRO ) )
   {
      if( IS_SET( ch->act, PLR_ANSI ) )
         send_ansi_title( ch );
      else
         send_ascii_title( ch );
   }
   else
   {
      write_to_buffer( d, "Press enter...\n\r", 0 );
   }
   d->connected = CON_PRESS_ENTER;
}

char *smaug_crypt( const char *pwd )
{
   md5_state_t state;
   md5_byte_t digest[17];
   static char passwd[17];
   unsigned int x;

   md5_init( &state );
   md5_append( &state, ( const md5_byte_t * )pwd, strlen( pwd ) );
   md5_finish( &state, digest );

   strncpy( passwd, ( const char * )digest, 16 );
   passwd[16] = '\0';

   /*
    * The listed exceptions below will fubar the MD5 authentication packets, so change them 
    */
   for( x = 0; x < strlen( passwd ); x++ )
   {
      if( passwd[x] == '\n' )
         passwd[x] = 'n';
      if( passwd[x] == '\r' )
         passwd[x] = 'r';
      if( passwd[x] == '\t' )
         passwd[x] = 't';
      if( passwd[x] == ' ' )
         passwd[x] = 's';
      if( ( int )passwd[x] == 11 )
         passwd[x] = 'x';
      if( ( int )passwd[x] == 12 )
         passwd[x] = 'X';
      if( passwd[x] == '~' )
         passwd[x] = '+';
      if( passwd[x] == EOF )
         passwd[x] = 'E';
   }
   return ( passwd );
}

/*
 * Deal with sockets that haven't logged in yet.
 */
void nanny( DESCRIPTOR_DATA * d, char *argument )
{
/*	extern int lang_array[];
	extern char *lang_names[];*/
   char buf[MAX_STRING_LENGTH];
   char arg[MAX_STRING_LENGTH];
   CHAR_DATA *ch;
   char *pwdnew;
   char *p;
   int iRace, iClass;
   BAN_DATA *pban;
/*    int iLang;*/
   bool fOld, chk;

   while( isspace( *argument ) )
      argument++;

   ch = d->character;

   switch ( d->connected )
   {

      default:
         bug( "Nanny: bad d->connected %d.", d->connected );
         close_socket( d, TRUE );
         return;

      case CON_GET_NAME:
         if( argument[0] == '\0' )
         {
            close_socket( d, FALSE );
            return;
         }

         argument[0] = UPPER( argument[0] );
         if( !check_parse_name( argument ) )
         {
            write_to_buffer( d, "Illegal name, try another.\n\rName: ", 0 );
            return;
         }

         if( !str_cmp( argument, "New" ) )
         {
            if( d->newstate == 0 )
            {
               /*
                * New player 
                */
               /*
                * Don't allow new players if DENY_NEW_PLAYERS is true 
                */
               if( sysdata.DENY_NEW_PLAYERS == TRUE )
               {
                  sprintf( buf, "The mud is currently preparing for a reboot.\n\r" );
                  write_to_buffer( d, buf, 0 );
                  sprintf( buf, "New players are not accepted during this time.\n\r" );
                  write_to_buffer( d, buf, 0 );
                  sprintf( buf, "Please try again in a few minutes.\n\r" );
                  write_to_buffer( d, buf, 0 );
                  close_socket( d, FALSE );
               }
               sprintf( buf, "\n\rChoosing a name is one of the most important parts of this game...\n\r"
                        "Make sure to pick a name appropriate to the character you are going\n\r"
                        "to role play, and be sure that it suits our Star Wars theme.\n\r"
                        "If the name you select is not acceptable, you will be asked to choose\n\r"
                        "another one.\n\r\n\rPlease choose a name for your character: " );
               write_to_buffer( d, buf, 0 );
               d->newstate++;
               d->connected = CON_GET_NAME;
               return;
            }
            else
            {
               write_to_buffer( d, "Illegal name, try another.\n\rName: ", 0 );
               return;
            }
         }

         if( check_playing( d, argument, FALSE ) == BERR )
         {
            write_to_buffer( d, "Name: ", 0 );
            return;
         }

         fOld = load_char_obj( d, argument, TRUE, FALSE );
         if( !d->character )
         {
            sprintf( log_buf, "Bad player file %s@%s.", argument, d->host );
            log_string( log_buf );
            write_to_buffer( d, "Your playerfile is corrupt...Please notify Thoric@mud.compulink.com.\n\r", 0 );
            close_socket( d, FALSE );
            return;
         }
         ch = d->character;

         for( pban = first_ban; pban; pban = pban->next )
         {
            if( ( !str_prefix( pban->name, d->host )
                  || !str_suffix( pban->name, d->host ) ) && pban->level >= ch->top_level )
            {
               write_to_buffer( d, "Your site has been banned from this Mud.\n\r", 0 );
               close_socket( d, FALSE );
               return;
            }
         }
         if( IS_SET( ch->act, PLR_DENY ) )
         {
            sprintf( log_buf, "Denying access to %s@%s.", argument, d->host );
            log_string_plus( log_buf, LOG_COMM, sysdata.log_level );
            if( d->newstate != 0 )
            {
               write_to_buffer( d, "That name is already taken.  Please choose another: ", 0 );
               d->connected = CON_GET_NAME;
               return;
            }
            write_to_buffer( d, "You are denied access.\n\r", 0 );
            close_socket( d, FALSE );
            return;
         }

         chk = check_reconnect( d, argument, FALSE );
         if( chk == BERR )
            return;

         if( chk )
         {
            fOld = TRUE;
         }
         else
         {
            if( wizlock && !IS_IMMORTAL( ch ) )
            {
               write_to_buffer( d, "The game is wizlocked.  Only immortals can connect now.\n\r", 0 );
               write_to_buffer( d, "Please try back later.\n\r", 0 );
               close_socket( d, FALSE );
               return;
            }
         }

         if( fOld )
         {
            if( d->newstate != 0 )
            {
               write_to_buffer( d, "That name is already taken.  Please choose another: ", 0 );
               d->connected = CON_GET_NAME;
               return;
            }
            /*
             * Old player 
             */
            write_to_buffer( d, "Password: ", 0 );
            write_to_buffer( d, echo_off_str, 0 );
            d->connected = CON_GET_OLD_PASSWORD;
            return;
         }
         else
         {
            write_to_buffer( d, "\n\rI don't recognize your name, you must be new here.\n\r\n\r", 0 );
            sprintf( buf, "Did I get that right, %s (Y/N)? ", argument );
            write_to_buffer( d, buf, 0 );
            d->connected = CON_CONFIRM_NEW_NAME;
            return;
         }
         break;

      case CON_GET_OLD_PASSWORD:
         write_to_buffer( d, "\n\r", 2 );

         if( str_cmp( smaug_crypt( argument ), ch->pcdata->pwd ) )
         {
            write_to_buffer( d, "Wrong password, disconnecting.\n\r", 0 );
            /*
             * clear descriptor pointer to get rid of bug message in log 
             */
            d->character->desc = NULL;
            close_socket( d, FALSE );
            return;
         }

         write_to_buffer( d, echo_on_str, 0 );

         if( check_playing( d, ch->name, TRUE ) )
            return;

         chk = check_reconnect( d, ch->name, TRUE );
         if( chk == BERR )
         {
            if( d->character && d->character->desc )
               d->character->desc = NULL;
            close_socket( d, FALSE );
            return;
         }
         if( chk == TRUE )
            return;

         if( check_multi( d, ch->name ) )
         {
            close_socket( d, FALSE );
            return;
         }

         strcpy( buf, ch->name );
         d->character->desc = NULL;
         free_char( d->character );
         fOld = load_char_obj( d, buf, FALSE, FALSE );
         ch = d->character;
         sprintf( log_buf, "%s (%s) has connected.", ch->name, d->host );
         if( ch->top_level < LEVEL_DEMI )
         {
            log_string_plus( log_buf, LOG_COMM, sysdata.log_level );
         }
         else
            log_string_plus( log_buf, LOG_COMM, ch->top_level );
         show_title( d );
         if( ch->pcdata->area )
            do_loadarea( ch, "" );


         break;

      case CON_CONFIRM_NEW_NAME:
         switch ( *argument )
         {
            case 'y':
            case 'Y':
               sprintf( buf, "\n\rMake sure to use a password that won't be easily guessed by someone else."
                        "\n\rPick a good password for %s: %s", ch->name, echo_off_str );
               write_to_buffer( d, buf, 0 );
               d->connected = CON_GET_NEW_PASSWORD;
               break;

            case 'n':
            case 'N':
               write_to_buffer( d, "Ok, what IS it, then? ", 0 );
               /*
                * clear descriptor pointer to get rid of bug message in log 
                */
               d->character->desc = NULL;
               free_char( d->character );
               d->character = NULL;
               d->connected = CON_GET_NAME;
               break;

            default:
               write_to_buffer( d, "Please type Yes or No. ", 0 );
               break;
         }
         break;

      case CON_GET_NEW_PASSWORD:
         write_to_buffer( d, "\n\r", 2 );

         if( strlen( argument ) < 5 )
         {
            write_to_buffer( d, "Password must be at least five characters long.\n\rPassword: ", 0 );
            return;
         }

         if( argument[0] == '!' )
         {
            write_to_buffer( d, "Password cannot begin with the '!' character.\n\rPassword: ", 0 );
            return;
         }

         pwdnew = smaug_crypt( argument );   /* MD5 Encryption */
         for( p = pwdnew; *p != '\0'; p++ )
         {
            if( *p == '~' )
            {
               write_to_buffer( d, "New password not acceptable, cannot use the ~ character.\n\rPassword: ", 0 );
               return;
            }
         }

         DISPOSE( ch->pcdata->pwd );
         ch->pcdata->pwd = str_dup( pwdnew );
         write_to_buffer( d, "\n\rPlease retype the password to confirm: ", 0 );
         d->connected = CON_CONFIRM_NEW_PASSWORD;
         break;

      case CON_CONFIRM_NEW_PASSWORD:
         write_to_buffer( d, "\n\r", 2 );

         if( str_cmp( smaug_crypt( argument ), ch->pcdata->pwd ) )
         {
            write_to_buffer( d, "Passwords don't match.\n\rRetype password: ", 0 );
            d->connected = CON_GET_NEW_PASSWORD;
            return;
         }

         write_to_buffer( d, echo_on_str, 0 );
         write_to_buffer( d, "\n\rWhat is your sex (M/F/N)? ", 0 );
         d->connected = CON_GET_NEW_SEX;
         break;

      case CON_GET_NEW_SEX:
         switch ( argument[0] )
         {
            case 'm':
            case 'M':
               ch->sex = SEX_MALE;
               break;
            case 'f':
            case 'F':
               ch->sex = SEX_FEMALE;
               break;
            case 'n':
            case 'N':
               ch->sex = SEX_NEUTRAL;
               break;
            default:
               write_to_buffer( d, "That's not a sex.\n\rWhat IS your sex? ", 0 );
               return;
         }


         write_to_buffer( d, "\n\rYou may choose from the following races, or type help [race] to learn more:\n\r[", 0 );
         buf[0] = '\0';
         for( iRace = 0; iRace < MAX_RACE; iRace++ )
         {
            if( race_table[iRace].race_name && race_table[iRace].race_name[0] != '\0' )
            {
               if( iRace > 0 )
               {
                  if( strlen( buf ) + strlen( race_table[iRace].race_name ) > 77 )
                  {
                     strcat( buf, "\n\r" );
                     write_to_buffer( d, buf, 0 );
                     buf[0] = '\0';
                  }
                  else
                     strcat( buf, " " );
               }
               strcat( buf, race_table[iRace].race_name );
            }
         }
         strcat( buf, "]\n\r: " );
         write_to_buffer( d, buf, 0 );
         d->connected = CON_GET_NEW_RACE;
         break;

      case CON_GET_NEW_RACE:
         argument = one_argument( argument, arg );
         if( !str_cmp( arg, "help" ) )
         {
            do_help( ch, argument );
            write_to_buffer( d, "Please choose a race: ", 0 );
            return;
         }


         for( iRace = 0; iRace < MAX_RACE; iRace++ )
         {
            if( toupper( arg[0] ) == toupper( race_table[iRace].race_name[0] )
                && !str_prefix( arg, race_table[iRace].race_name ) )
            {
               ch->race = iRace;
               break;
            }
         }

         if( iRace == MAX_RACE || !race_table[iRace].race_name || race_table[iRace].race_name[0] == '\0' )
         {
            write_to_buffer( d, "That's not a race.\n\rWhat IS your race? ", 0 );
            return;
         }

         write_to_buffer( d, "\n\rPlease choose a main ability from the folowing classes:\n\r[", 0 );
         buf[0] = '\0';
         for( iClass = 0; iClass < MAX_ABILITY; iClass++ )
         {
            if( ability_name[iClass] && ability_name[iClass][0] != '\0' )
            {
               if( iClass > 0 )
               {
                  if( strlen( buf ) + strlen( ability_name[iClass] ) > 77 )
                  {
                     strcat( buf, "\n\r" );
                     write_to_buffer( d, buf, 0 );
                     buf[0] = '\0';
                  }
                  else
                     strcat( buf, " " );
               }
               strcat( buf, ability_name[iClass] );
            }
         }
         strcat( buf, "]\n\r: " );
         write_to_buffer( d, buf, 0 );
         d->connected = CON_GET_NEW_CLASS;
         break;

      case CON_GET_NEW_CLASS:
         argument = one_argument( argument, arg );
         if( !str_cmp( arg, "help" ) )
         {
            do_help( ch, argument );
            write_to_buffer( d, "Please choose an ability class: ", 0 );
            return;
         }


         for( iClass = 0; iClass < MAX_ABILITY; iClass++ )
         {
            if( toupper( arg[0] ) == toupper( ability_name[iClass][0] ) && !str_prefix( arg, ability_name[iClass] ) )
            {
               ch->main_ability = iClass;
               break;
            }
         }

         if( iClass == MAX_ABILITY || !ability_name[iClass] || ability_name[iClass][0] == '\0' )
         {
            write_to_buffer( d, "That's not a skill class.\n\rWhat IS it going to be? ", 0 );
            return;
         }

         write_to_buffer( d, "\n\rRolling stats....\n\r", 0 );

      case CON_ROLL_STATS:

         ch->perm_str = number_range( 1, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
         ch->perm_int = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
         ch->perm_wis = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
         ch->perm_dex = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
         ch->perm_con = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
         ch->perm_cha = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );

         ch->perm_str += race_table[ch->race].str_plus;
         ch->perm_int += race_table[ch->race].int_plus;
         ch->perm_wis += race_table[ch->race].wis_plus;
         ch->perm_dex += race_table[ch->race].dex_plus;
         ch->perm_con += race_table[ch->race].con_plus;
         ch->perm_cha += race_table[ch->race].cha_plus;

         sprintf( buf, "\n\rSTR: %d  INT: %d  WIS: %d  DEX: %d  CON: %d  CHA: %d\n\r",
                  ch->perm_str, ch->perm_int, ch->perm_wis, ch->perm_dex, ch->perm_con, ch->perm_cha );

         write_to_buffer( d, buf, 0 );
         write_to_buffer( d, "\n\rAre these stats OK?. ", 0 );
         d->connected = CON_STATS_OK;
         break;

      case CON_STATS_OK:

         switch ( argument[0] )
         {
            case 'y':
            case 'Y':
               break;
            case 'n':
            case 'N':
               ch->perm_str = number_range( 1, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
               ch->perm_int = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
               ch->perm_wis = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
               ch->perm_dex = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
               ch->perm_con = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );
               ch->perm_cha = number_range( 3, 6 ) + number_range( 1, 6 ) + number_range( 1, 6 );

               ch->perm_str += race_table[ch->race].str_plus;
               ch->perm_int += race_table[ch->race].int_plus;
               ch->perm_wis += race_table[ch->race].wis_plus;
               ch->perm_dex += race_table[ch->race].dex_plus;
               ch->perm_con += race_table[ch->race].con_plus;
               ch->perm_cha += race_table[ch->race].cha_plus;

               sprintf( buf, "\n\rSTR: %d  INT: %d  WIS: %d  DEX: %d  CON: %d  CHA: %d\n\r",
                        ch->perm_str, ch->perm_int, ch->perm_wis, ch->perm_dex, ch->perm_con, ch->perm_cha );

               write_to_buffer( d, buf, 0 );
               write_to_buffer( d, "\n\rOK?. ", 0 );
               return;
            default:
               write_to_buffer( d, "Invalid selection.\n\rYES or NO? ", 0 );
               return;
         }

         write_to_buffer( d, "\n\rWould you like ANSI or no graphic/color support, (R/A/N)? ", 0 );
         d->connected = CON_GET_WANT_RIPANSI;
         break;

      case CON_GET_WANT_RIPANSI:
         switch ( argument[0] )
         {
            case 'a':
            case 'A':
               SET_BIT( ch->act, PLR_ANSI );
               break;
            case 'n':
            case 'N':
               break;
            default:
               write_to_buffer( d, "Invalid selection.\n\rANSI or NONE? ", 0 );
               return;
         }
         write_to_buffer( d, "Does your mud client have the Mud Sound Protocol? ", 0 );
         d->connected = CON_GET_MSP;
         break;


      case CON_GET_MSP:
         switch ( argument[0] )
         {
            case 'y':
            case 'Y':
               SET_BIT( ch->act, PLR_SOUND );
               break;
            case 'n':
            case 'N':
               break;
            default:
               write_to_buffer( d, "Invalid selection.\n\rYES or NO? ", 0 );
               return;
         }
/*
	if ( !sysdata.WAIT_FOR_AUTH )
	{
*/ sprintf( log_buf, "%s@%s new %s.", ch->name, d->host,
               race_table[ch->race].race_name );
         log_string_plus( log_buf, LOG_COMM, sysdata.log_level );
         to_channel( log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL );
         write_to_buffer( d, "Press [ENTER] ", 0 );
         show_title( d );
         {
            int ability;

            for( ability = 0; ability < MAX_ABILITY; ability++ )
               ch->skill_level[ability] = 0;
         }
         ch->top_level = 0;
         ch->position = POS_STANDING;
         d->connected = CON_PRESS_ENTER;
         return;
         break;
/*	}

	write_to_buffer( d, "\n\rYou now have to wait for a god to authorize you... please be patient...\n\r", 0 );
	sprintf( log_buf, "(1) %s@%s new %s applying for authorization...",
				ch->name, d->host,
				race_table[ch->race].race_name);
	log_string( log_buf );
	to_channel( log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL );
	d->connected = CON_WAIT_1;
	break;

     case CON_WAIT_1:
	write_to_buffer( d, "\n\rTwo more tries... please be patient...\n\r", 0 );
	sprintf( log_buf, "(2) %s@%s new %s applying for authorization...",
				ch->name, d->host,
				race_table[ch->race].race_name);
	log_string( log_buf );
	to_channel( log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL );
	d->connected = CON_WAIT_2;
	break;

     case CON_WAIT_2:
	write_to_buffer( d, "\n\rThis is your last try...\n\r", 0 );
	sprintf( log_buf, "(3) %s@%s new %s applying for authorization...",
				ch->name, d->host,
				race_table[ch->race].race_name);
	log_string( log_buf );
	to_channel( log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL );
	d->connected = CON_WAIT_3;
	break;

    case CON_WAIT_3:
	write_to_buffer( d, "Sorry... try again later.\n\r", 0 );
	close_socket( d, FALSE );
	return;
	break;

    case CON_ACCEPTED:

	sprintf( log_buf, "%s@%s new %s.", ch->name, d->host,
				race_table[ch->race].race_name);
	log_string_plus( log_buf, LOG_COMM, sysdata.log_level );
	to_channel( log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL );
	write_to_buffer( d, "\n\r", 2 );
	show_title(d);
	    {
	       int ability;
	       
	       for ( ability =0 ; ability < MAX_ABILITY ; ability++ )
	          ch->skill_level[ability] = 0;
	    }
	ch->top_level = 0;
	ch->position = POS_STANDING;
	d->connected = CON_PRESS_ENTER;
	break;
*/
      case CON_PRESS_ENTER:
         if( IS_SET( ch->act, PLR_ANSI ) )
            send_to_pager( "\033[2J", ch );
         else
            send_to_pager( "\014", ch );
         if( IS_IMMORTAL( ch ) )
         {
            send_to_pager( "&WImmortal Message of the Day&w\n\r", ch );
            do_help( ch, "imotd" );
         }
         if( ch->top_level > 0 )
         {
            send_to_pager( "\n\r&WMessage of the Day&w\n\r", ch );
            do_help( ch, "motd" );
         }
         if( ch->top_level >= 100 )
         {
            send_to_pager( "\n\r&WAvatar Message of the Day&w\n\r", ch );
            do_help( ch, "amotd" );
         }
         if( ch->top_level == 0 )
            do_help( ch, "nmotd" );
         send_to_pager( "\n\r&WPress [ENTER] &Y", ch );
         d->connected = CON_READ_MOTD;
         break;

      case CON_READ_MOTD:
         write_to_buffer( d, "\n\rWelcome to Star Wars Reality...\n\r\n\r", 0 );
         add_char( ch );
         d->connected = CON_PLAYING;

         if( !IS_NPC( ch ) && IS_SET( ch->act, PLR_SOUND ) )
            send_to_char( "!!MUSIC(starwars.mid V=100)", ch );


         if( ch->top_level == 0 )
         {
            OBJ_DATA *obj;
            int iLang;

            ch->pcdata->clan = NULL;

            ch->perm_lck = number_range( 6, 18 );
            ch->perm_frc = number_range( -2000, 20 );
            ch->affected_by = race_table[ch->race].affected;
            ch->perm_lck += race_table[ch->race].lck_plus;
            ch->perm_frc += race_table[ch->race].frc_plus;

            if( ch->race == RACE_DUINUOGWUIN || ch->main_ability == FORCE_ABILITY )
               ch->perm_frc = URANGE( 1, ch->perm_frc, 20 );
            else
               ch->perm_frc = URANGE( 0, ch->perm_frc, 20 );

            /*
             * took out automaticly knowing common
             * if ( (iLang = skill_lookup( "common" )) < 0 )
             * bug( "Nanny: cannot find common language." );
             * else
             * ch->pcdata->learned[iLang] = 100;
             */

            for( iLang = 0; lang_array[iLang] != LANG_UNKNOWN; iLang++ )
               if( lang_array[iLang] == race_table[ch->race].language )
                  break;
            if( lang_array[iLang] == LANG_UNKNOWN )
               bug( "Nanny: invalid racial language." );
            else
            {
               if( ( iLang = skill_lookup( lang_names[iLang] ) ) < 0 )
                  bug( "Nanny: cannot find racial language." );
               else
               {
                  ch->pcdata->learned[iLang] = 100;
                  ch->speaking = race_table[ch->race].language;
                  if( ch->race == RACE_QUARREN && ( iLang = skill_lookup( "quarren" ) ) >= 0 )
                  {
                     ch->pcdata->learned[iLang] = 100;
                     SET_BIT( ch->speaks, LANG_QUARREN );
                  }
                  if( ch->race == RACE_MON_CALAMARI && ( iLang = skill_lookup( "common" ) ) >= 0 )
                     ch->pcdata->learned[iLang] = 100;

               }
            }

            /*
             * ch->resist           += race_table[ch->race].resist;    drats 
             */
            /*
             * ch->susceptible     += race_table[ch->race].suscept;    drats 
             */

            reset_colors( ch );
            name_stamp_stats( ch );

            {
               int ability;

               for( ability = 0; ability < MAX_ABILITY; ability++ )
               {
                  ch->skill_level[ability] = 1;
                  ch->experience[ability] = 0;
               }
            }
            ch->top_level = 1;
            ch->hit = ch->max_hit;
            ch->hit += race_table[ch->race].hit;
            ch->move = ch->max_move;
            if( ch->perm_frc > 0 )
               ch->max_mana = 100 + 100 * ch->perm_frc;
            else
               ch->max_mana = 0;
            ch->mana = ch->max_mana;
            sprintf( buf, "%s the %s", ch->name, race_table[ch->race].race_name );
            set_title( ch, buf );

            /*
             * Added by Narn.  Start new characters with autoexit and autgold
             * already turned on.  Very few people don't use those. 
             */
            SET_BIT( ch->act, PLR_AUTOGOLD );
            SET_BIT( ch->act, PLR_AUTOEXIT );

            /*
             * New players don't have to earn some eq 
             */

            obj = create_object( get_obj_index( OBJ_VNUM_SCHOOL_BANNER ), 0 );
            obj_to_char( obj, ch );
            equip_char( ch, obj, WEAR_LIGHT );

            /*
             * armor they do though
             * obj = create_object( get_obj_index(OBJ_VNUM_SCHOOL_VEST), 0 );
             * obj_to_char( obj, ch );
             * equip_char( ch, obj, WEAR_BODY );
             * 
             * obj = create_object( get_obj_index(OBJ_VNUM_SCHOOL_SHIELD), 0 );
             * obj_to_char( obj, ch );
             * equip_char( ch, obj, WEAR_SHIELD );
             */

            obj = create_object( get_obj_index( OBJ_VNUM_SCHOOL_DAGGER ), 0 );
            obj_to_char( obj, ch );
            equip_char( ch, obj, WEAR_WIELD );

            /*
             * comlink 
             */

            {
               OBJ_INDEX_DATA *obj_ind = get_obj_index( 10424 );
               if( obj_ind != NULL )
               {
                  obj = create_object( obj_ind, 0 );
                  obj_to_char( obj, ch );
               }
            }

            if( !sysdata.WAIT_FOR_AUTH )
            {
               char_to_room( ch, get_room_index( ROOM_VNUM_SCHOOL ) );
               ch->pcdata->auth_state = 3;
            }
            else
            {
               char_to_room( ch, get_room_index( ROOM_VNUM_SCHOOL ) );
               ch->pcdata->auth_state = 1;
               SET_BIT( ch->pcdata->flags, PCFLAG_UNAUTHED );
            }
         }
         else if( !IS_IMMORTAL( ch ) && ch->pcdata->release_date > current_time )
         {
            char_to_room( ch, get_room_index( 6 ) );
         }
         else if( ch->in_room && !IS_IMMORTAL( ch )
                  && !IS_SET( ch->in_room->room_flags, ROOM_SPACECRAFT ) && ch->in_room != get_room_index( 6 ) )
         {
            char_to_room( ch, ch->in_room );
         }
         else if( ch->in_room && !IS_IMMORTAL( ch )
                  && IS_SET( ch->in_room->room_flags, ROOM_SPACECRAFT ) && ch->in_room != get_room_index( 6 ) )
         {
            SHIP_DATA *ship;

            for( ship = first_ship; ship; ship = ship->next )
               if( ch->in_room->vnum >= ship->firstroom && ch->in_room->vnum <= ship->lastroom )
                  if( ship->class != SHIP_PLATFORM || ship->starsystem )
                     char_to_room( ch, ch->in_room );
         }
         else
         {
            char_to_room( ch, get_room_index( wherehome( ch ) ) );
         }

         if( get_timer( ch, TIMER_SHOVEDRAG ) > 0 )
            remove_timer( ch, TIMER_SHOVEDRAG );

         if( get_timer( ch, TIMER_PKILLED ) > 0 )
            remove_timer( ch, TIMER_PKILLED );
         if( ch->plr_home != NULL )
         {
            char filename[256];
            FILE *fph;
            ROOM_INDEX_DATA *storeroom = ch->plr_home;
            OBJ_DATA *obj;
            OBJ_DATA *obj_next;

            for( obj = storeroom->first_content; obj; obj = obj_next )
            {
               obj_next = obj->next_content;
               extract_obj( obj );
            }

            sprintf( filename, "%s%c/%s.home", PLAYER_DIR, tolower( ch->name[0] ), capitalize( ch->name ) );
            if( ( fph = fopen( filename, "r" ) ) != NULL )
            {
               int iNest;
               bool found;
               OBJ_DATA *tobj, *tobj_next;

               rset_supermob( storeroom );
               for( iNest = 0; iNest < MAX_NEST; iNest++ )
                  rgObjNest[iNest] = NULL;

               found = TRUE;
               for( ;; )
               {
                  char letter;
                  char *word;

                  letter = fread_letter( fph );
                  if( letter == '*' )
                  {
                     fread_to_eol( fph );
                     continue;
                  }

                  if( letter != '#' )
                  {
                     bug( "Load_plr_home: # not found.", 0 );
                     bug( ch->name, 0 );
                     break;
                  }

                  word = fread_word( fph );
                  if( !str_cmp( word, "OBJECT" ) ) /* Objects  */
                     fread_obj( supermob, fph, OS_CARRY );
                  else if( !str_cmp( word, "END" ) )  /* Done     */
                     break;
                  else
                  {
                     bug( "Load_plr_home: bad section.", 0 );
                     bug( ch->name, 0 );
                     break;
                  }
               }

               fclose( fph );

               for( tobj = supermob->first_carrying; tobj; tobj = tobj_next )
               {
                  tobj_next = tobj->next_content;
                  obj_from_char( tobj );
                  obj_to_room( tobj, storeroom );
               }

               release_supermob(  );

            }
         }

         act( AT_ACTION, "$n has entered the game.", ch, NULL, NULL, TO_ROOM );
         do_look( ch, "auto" );
         mail_count( ch );
         break;

         /*
          * Far too many possible screwups if we do it this way. -- Altrag 
          */
/*        case CON_NEW_LANGUAGE:
        for ( iLang = 0; lang_array[iLang] != LANG_UNKNOWN; iLang++ )
		if ( !str_prefix( argument, lang_names[iLang] ) )
			if ( can_learn_lang( ch, lang_array[iLang] ) )
			{
				add_char( ch );
				SET_BIT( ch->speaks, lang_array[iLang] );
				set_char_color( AT_SAY, ch );
				ch_printf( ch, "You can now speak %s.\n\r", lang_names[iLang] );
				d->connected = CON_PLAYING;
				return;
			}
	set_char_color( AT_SAY, ch );
	write_to_buffer( d, "You may not learn that language.  Please choose another.\n\r"
				  "New language: ", 0 );
	break;*/
   }

   return;
}



/*
 * Parse a name for acceptability.
 */
bool check_parse_name( char *name )
{
   /*
    * Reserved words.
    */
   if( is_name
       ( name,
         "all auto someone immortal self god supreme demigod dog guard cityguard cat cornholio spock hicaine hithoric death ass fuck shit piss crap quit" ) )
      return FALSE;

   /*
    * Length restrictions.
    */
   if( strlen( name ) < 3 )
      return FALSE;

   if( strlen( name ) > 12 )
      return FALSE;

   /*
    * Alphanumerics only.
    * Lock out IllIll twits.
    */
   {
      char *pc;
      bool fIll;

      fIll = TRUE;
      for( pc = name; *pc != '\0'; pc++ )
      {
         if( !isalpha( *pc ) )
            return FALSE;
         if( LOWER( *pc ) != 'i' && LOWER( *pc ) != 'l' )
            fIll = FALSE;
      }

      if( fIll )
         return FALSE;
   }

   /*
    * Code that followed here used to prevent players from naming
    * themselves after mobs... this caused much havoc when new areas
    * would go in...
    */

   return TRUE;
}



/*
 * Look for link-dead player to reconnect.
 */
bool check_reconnect( DESCRIPTOR_DATA * d, char *name, bool fConn )
{
   CHAR_DATA *ch;

   for( ch = first_char; ch; ch = ch->next )
   {
      if( !IS_NPC( ch ) && ( !fConn || !ch->desc ) && ch->name && !str_cmp( name, ch->name ) )
      {
         if( fConn && ch->switched )
         {
            write_to_buffer( d, "Already playing.\n\rName: ", 0 );
            d->connected = CON_GET_NAME;
            if( d->character )
            {
               /*
                * clear descriptor pointer to get rid of bug message in log 
                */
               d->character->desc = NULL;
               free_char( d->character );
               d->character = NULL;
            }
            return BERR;
         }
         if( fConn == FALSE )
         {
            DISPOSE( d->character->pcdata->pwd );
            d->character->pcdata->pwd = str_dup( ch->pcdata->pwd );
         }
         else
         {
            /*
             * clear descriptor pointer to get rid of bug message in log 
             */
            d->character->desc = NULL;
            free_char( d->character );
            d->character = ch;
            ch->desc = d;
            ch->timer = 0;
            send_to_char( "Reconnecting.\n\r", ch );
            act( AT_ACTION, "$n has reconnected.", ch, NULL, NULL, TO_ROOM );
            sprintf( log_buf, "%s (%s) reconnected.", ch->name, d->host );
            log_string_plus( log_buf, LOG_COMM, UMAX( sysdata.log_level, ch->top_level ) );
            d->connected = CON_PLAYING;
         }
         return TRUE;
      }
   }

   return FALSE;
}



/*
 * Check if already playing.
 */

bool check_multi( DESCRIPTOR_DATA * d, char *name )
{
   DESCRIPTOR_DATA *dold;

   for( dold = first_descriptor; dold; dold = dold->next )
   {
      if( dold != d
          && ( dold->character || dold->original )
          && str_cmp( name, dold->original
                      ? dold->original->name : dold->character->name ) && !str_cmp( dold->host, d->host ) )
      {
         const char *ok = "194.234.177";
         const char *ok2 = "209.183.133.229";
         int iloop;

         if( get_trust( d->character ) >= LEVEL_SUPREME
             || get_trust( dold->original ? dold->original : dold->character ) >= LEVEL_SUPREME )
            return FALSE;
         for( iloop = 0; iloop < 11; iloop++ )
         {
            if( ok[iloop] != d->host[iloop] )
               break;
         }
         if( iloop >= 10 )
            return FALSE;
         for( iloop = 0; iloop < 11; iloop++ )
         {
            if( ok2[iloop] != d->host[iloop] )
               break;
         }
         if( iloop >= 10 )
            return FALSE;
         write_to_buffer( d, "Sorry multi-playing is not allowed ... have you other character quit first.\n\r", 0 );
         sprintf( log_buf, "%s attempting to multiplay with %s.",
                  dold->original ? dold->original->name : dold->character->name, d->character->name );
         log_string_plus( log_buf, LOG_COMM, sysdata.log_level );
         d->character = NULL;
         free_char( d->character );
         return TRUE;
      }
   }

   return FALSE;

}

bool check_playing( DESCRIPTOR_DATA * d, char *name, bool kick )
{
   CHAR_DATA *ch;

   DESCRIPTOR_DATA *dold;
   int cstate;

   for( dold = first_descriptor; dold; dold = dold->next )
   {
      if( dold != d
          && ( dold->character || dold->original )
          && !str_cmp( name, dold->original ? dold->original->name : dold->character->name ) )
      {
         cstate = dold->connected;
         ch = dold->original ? dold->original : dold->character;
         if( !ch->name || ( cstate != CON_PLAYING && cstate != CON_EDITING ) )
         {
            write_to_buffer( d, "Already connected - try again.\n\r", 0 );
            sprintf( log_buf, "%s already connected.", ch->name );
            log_string_plus( log_buf, LOG_COMM, sysdata.log_level );
            return BERR;
         }
         if( !kick )
            return TRUE;
         write_to_buffer( d, "Already playing... Kicking off old connection.\n\r", 0 );
         write_to_buffer( dold, "Kicking off old connection... bye!\n\r", 0 );
         close_socket( dold, FALSE );
         /*
          * clear descriptor pointer to get rid of bug message in log 
          */
         d->character->desc = NULL;
         free_char( d->character );
         d->character = ch;
         ch->desc = d;
         ch->timer = 0;
         if( ch->switched )
            do_return( ch->switched, "" );
         ch->switched = NULL;
         send_to_char( "Reconnecting.\n\r", ch );
         act( AT_ACTION, "$n has reconnected, kicking off old link.", ch, NULL, NULL, TO_ROOM );
         sprintf( log_buf, "%s@%s reconnected, kicking off old link.", ch->name, d->host );
         log_string_plus( log_buf, LOG_COMM, UMAX( sysdata.log_level, ch->top_level ) );
         d->connected = cstate;
         return TRUE;
      }
   }

   return FALSE;
}



void stop_idling( CHAR_DATA * ch )
{
   if( !ch
       || !ch->desc
       || ch->desc->connected != CON_PLAYING || !ch->was_in_room || ch->in_room != get_room_index( ROOM_VNUM_LIMBO ) )
      return;

   ch->timer = 0;
   char_from_room( ch );
   char_to_room( ch, ch->was_in_room );
   ch->was_in_room = NULL;
   act( AT_ACTION, "$n has returned from the void.", ch, NULL, NULL, TO_ROOM );
   return;
}

char *obj_short( OBJ_DATA * obj )
{
   static char buf[MAX_STRING_LENGTH];

   if( obj->count > 1 )
   {
      sprintf( buf, "%s (%d)", obj->short_descr, obj->count );
      return buf;
   }
   return obj->short_descr;
}

/*
 * The primary output interface for formatted output.
 */
/* Major overhaul. -- Alty */
#define NAME(ch)	(IS_NPC(ch) ? ch->short_descr : ch->name)
char *act_string( const char *format, CHAR_DATA * to, CHAR_DATA * ch, void *arg1, void *arg2 )
{
   static char *const he_she[] = { "it", "he", "she" };
   static char *const him_her[] = { "it", "him", "her" };
   static char *const his_her[] = { "its", "his", "her" };
   static char buf[MAX_STRING_LENGTH];
   char fname[MAX_INPUT_LENGTH];
   char *point = buf;
   const char *str = format;
   const char *i;
   CHAR_DATA *vch = ( CHAR_DATA * ) arg2;
   OBJ_DATA *obj1 = ( OBJ_DATA * ) arg1;
   OBJ_DATA *obj2 = ( OBJ_DATA * ) arg2;

   while( *str != '\0' )
   {
      if( *str != '$' )
      {
         *point++ = *str++;
         continue;
      }
      ++str;
      if( !arg2 && *str >= 'A' && *str <= 'Z' )
      {
         bug( "Act: missing arg2 for code %c:", *str );
         bug( format );
         i = " <@@@> ";
      }
      else
      {
         switch ( *str )
         {
            default:
               bug( "Act: bad code %c.", *str );
               i = " <@@@> ";
               break;
            case 't':
               i = ( char * )arg1;
               break;
            case 'T':
               i = ( char * )arg2;
               break;
            case 'n':
               i = ( to ? PERS( ch, to ) : NAME( ch ) );
               break;
            case 'N':
               i = ( to ? PERS( vch, to ) : NAME( vch ) );
               break;
            case 'e':
               if( ch->sex > 2 || ch->sex < 0 )
               {
                  bug( "act_string: player %s has sex set at %d!", ch->name, ch->sex );
                  i = "it";
               }
               else
                  i = he_she[URANGE( 0, ch->sex, 2 )];
               break;
            case 'E':
               if( vch->sex > 2 || vch->sex < 0 )
               {
                  bug( "act_string: player %s has sex set at %d!", vch->name, vch->sex );
                  i = "it";
               }
               else
                  i = he_she[URANGE( 0, vch->sex, 2 )];
               break;
            case 'm':
               if( ch->sex > 2 || ch->sex < 0 )
               {
                  bug( "act_string: player %s has sex set at %d!", ch->name, ch->sex );
                  i = "it";
               }
               else
                  i = him_her[URANGE( 0, ch->sex, 2 )];
               break;
            case 'M':
               if( vch->sex > 2 || vch->sex < 0 )
               {
                  bug( "act_string: player %s has sex set at %d!", vch->name, vch->sex );
                  i = "it";
               }
               else
                  i = him_her[URANGE( 0, vch->sex, 2 )];
               break;
            case 's':
               if( ch->sex > 2 || ch->sex < 0 )
               {
                  bug( "act_string: player %s has sex set at %d!", ch->name, ch->sex );
                  i = "its";
               }
               else
                  i = his_her[URANGE( 0, ch->sex, 2 )];
               break;
            case 'S':
               if( vch->sex > 2 || vch->sex < 0 )
               {
                  bug( "act_string: player %s has sex set at %d!", vch->name, vch->sex );
                  i = "its";
               }
               else
                  i = his_her[URANGE( 0, vch->sex, 2 )];
               break;
            case 'q':
               i = ( to == ch ) ? "" : "s";
               break;
            case 'Q':
               i = ( to == ch ) ? "your" : his_her[URANGE( 0, ch->sex, 2 )];
               break;
            case 'p':
               i = ( !to || can_see_obj( to, obj1 ) ? obj_short( obj1 ) : "something" );
               break;
            case 'P':
               i = ( !to || can_see_obj( to, obj2 ) ? obj_short( obj2 ) : "something" );
               break;
            case 'd':
               if( !arg2 || ( ( char * )arg2 )[0] == '\0' )
                  i = "door";
               else
               {
                  one_argument( ( char * )arg2, fname );
                  i = fname;
               }
               break;
         }
      }
      ++str;
      while( ( *point = *i ) != '\0' )
         ++point, ++i;
   }
   strcpy( point, "\n\r" );
   buf[0] = UPPER( buf[0] );
   return buf;
}

#undef NAME

void act( short AType, const char *format, CHAR_DATA * ch, void *arg1, void *arg2, int type )
{
   char *txt;
   CHAR_DATA *to;
   CHAR_DATA *vch = ( CHAR_DATA * ) arg2;

   /*
    * Discard null and zero-length messages.
    */
   if( !format || format[0] == '\0' )
      return;

   if( !ch )
   {
      bug( "Act: null ch. (%s)", format );
      return;
   }

   if( !ch->in_room )
      to = NULL;
   else if( type == TO_CHAR )
      to = ch;
   else
      to = ch->in_room->first_person;

   /*
    * ACT_SECRETIVE handling
    */
   if( IS_NPC( ch ) && IS_SET( ch->act, ACT_SECRETIVE ) && type != TO_CHAR )
      return;

   if( type == TO_VICT )
   {
      if( !vch )
      {
         bug( "Act: null vch with TO_VICT." );
         bug( "%s (%s)", ch->name, format );
         return;
      }
      if( !vch->in_room )
      {
         bug( "Act: vch in NULL room!" );
         bug( "%s -> %s (%s)", ch->name, vch->name, format );
         return;
      }
      to = vch;
/*	to = vch->in_room->first_person;*/
   }

   if( MOBtrigger && type != TO_CHAR && type != TO_VICT && to )
   {
      OBJ_DATA *to_obj;

      txt = act_string( format, NULL, ch, arg1, arg2 );
      if( IS_SET( to->in_room->progtypes, ACT_PROG ) )
         rprog_act_trigger( txt, to->in_room, ch, ( OBJ_DATA * ) arg1, ( void * )arg2 );
      for( to_obj = to->in_room->first_content; to_obj; to_obj = to_obj->next_content )
         if( IS_SET( to_obj->pIndexData->progtypes, ACT_PROG ) )
            oprog_act_trigger( txt, to_obj, ch, ( OBJ_DATA * ) arg1, ( void * )arg2 );
   }

   /*
    * Anyone feel like telling me the point of looping through the whole
    * room when we're only sending to one char anyways..? -- Alty 
    */
   for( ; to; to = ( type == TO_CHAR || type == TO_VICT ) ? NULL : to->next_in_room )
   {
      if( ( !to->desc && ( IS_NPC( to ) && !IS_SET( to->pIndexData->progtypes, ACT_PROG ) ) ) || !IS_AWAKE( to ) )
         continue;

      if( type == TO_CHAR && to != ch )
         continue;
      if( type == TO_VICT && ( to != vch || to == ch ) )
         continue;
      if( type == TO_ROOM && to == ch )
         continue;
      if( type == TO_NOTVICT && ( to == ch || to == vch ) )
         continue;

      txt = act_string( format, to, ch, arg1, arg2 );
      if( to->desc )
      {
         set_char_color( AType, to );
         send_to_char( txt, to );
      }
      if( MOBtrigger )
      {
         /*
          * Note: use original string, not string with ANSI. -- Alty 
          */
         mprog_act_trigger( txt, to, ch, ( OBJ_DATA * ) arg1, ( void * )arg2 );
      }
   }
   MOBtrigger = TRUE;
   return;
}

void do_name( CHAR_DATA * ch, char *argument )
{
   char fname[1024];
   struct stat fst;
   CHAR_DATA *tmp;

   if( !NOT_AUTHED( ch ) || ch->pcdata->auth_state != 2 )
   {
      send_to_char( "Huh?\n\r", ch );
      return;
   }

   argument[0] = UPPER( argument[0] );

   if( !check_parse_name( argument ) )
   {
      send_to_char( "Illegal name, try another.\n\r", ch );
      return;
   }

   if( !str_cmp( ch->name, argument ) )
   {
      send_to_char( "That's already your name!\n\r", ch );
      return;
   }

   for( tmp = first_char; tmp; tmp = tmp->next )
   {
      if( !str_cmp( argument, tmp->name ) )
         break;
   }

   if( tmp )
   {
      send_to_char( "That name is already taken.  Please choose another.\n\r", ch );
      return;
   }

   sprintf( fname, "%s%c/%s", PLAYER_DIR, tolower( argument[0] ), capitalize( argument ) );
   if( stat( fname, &fst ) != -1 )
   {
      send_to_char( "That name is already taken.  Please choose another.\n\r", ch );
      return;
   }

   STRFREE( ch->name );
   ch->name = STRALLOC( argument );
   send_to_char( "Your name has been changed.  Please apply again.\n\r", ch );
   ch->pcdata->auth_state = 0;
   return;
}

char *default_prompt( CHAR_DATA * ch )
{
   static char buf[MAX_STRING_LENGTH];
   strcpy( buf, "" );
   if( ch->skill_level[FORCE_ABILITY] > 1 || get_trust( ch ) >= LEVEL_IMMORTAL )
      strcat( buf, "&pForce:&P%m/&p%M  &pAlign:&P%a\n\r" );
   strcat( buf, "&BHealth:&C%h&B/%H  &BMovement:&C%v&B/%V" );
   strcat( buf, "&C >&w" );
   return buf;
}

int getcolor( char clr )
{
   static const char colors[16] = "xrgObpcwzRGYBPCW";
   int r;

   for( r = 0; r < 16; r++ )
      if( clr == colors[r] )
         return r;
   return -1;
}

void display_prompt( DESCRIPTOR_DATA * d )
{
   CHAR_DATA *ch = d->character;
   CHAR_DATA *och = ( d->original ? d->original : d->character );
   bool ansi = ( !IS_NPC( och ) && IS_SET( och->act, PLR_ANSI ) );
   const char *prompt;
   char buf[MAX_STRING_LENGTH];
   char *pbuf = buf;
   int pstat;

   if( !ch )
   {
      bug( "display_prompt: NULL ch" );
      return;
   }

   if( !IS_NPC( ch ) && ch->substate != SUB_NONE && ch->pcdata->subprompt && ch->pcdata->subprompt[0] != '\0' )
      prompt = ch->pcdata->subprompt;
   else if( IS_NPC( ch ) || !ch->pcdata->prompt || !*ch->pcdata->prompt )
      prompt = default_prompt( ch );
   else
      prompt = ch->pcdata->prompt;

   if( ansi )
   {
      strcpy( pbuf, ANSI_RESET );
      d->prevcolor = 0x08;
      pbuf += 4;
   }
   for( ; *prompt; prompt++ )
   {
      /*
       * '%' = prompt commands
       * Note: foreground changes will revert background to 0 (black)
       */
      if( *prompt != '%' )
      {
         *( pbuf++ ) = *prompt;
         continue;
      }
      ++prompt;
      if( !*prompt )
         break;
      if( *prompt == *( prompt - 1 ) )
      {
         *( pbuf++ ) = *prompt;
         continue;
      }
      switch ( *( prompt - 1 ) )
      {
         default:
            bug( "Display_prompt: bad command char '%c'.", *( prompt - 1 ) );
            break;
         case '%':
            *pbuf = '\0';
            pstat = 0x80000000;
            switch ( *prompt )
            {
               case '%':
                  *pbuf++ = '%';
                  *pbuf = '\0';
                  break;
               case 'a':
                  if( ch->top_level >= 10 )
                     pstat = ch->alignment;
                  else if( IS_GOOD( ch ) )
                     strcpy( pbuf, "good" );
                  else if( IS_EVIL( ch ) )
                     strcpy( pbuf, "evil" );
                  else
                     strcpy( pbuf, "neutral" );
                  break;
               case 'h':
                  pstat = ch->hit;
                  break;
               case 'H':
                  pstat = ch->max_hit;
                  break;
               case 'm':
                  if( IS_IMMORTAL( ch ) || ch->skill_level[FORCE_ABILITY] > 1 )
                     pstat = ch->mana;
                  else
                     pstat = 0;
                  break;
               case 'M':
                  if( IS_IMMORTAL( ch ) || ch->skill_level[FORCE_ABILITY] > 1 )
                     pstat = ch->max_mana;
                  else
                     pstat = 0;
                  break;
               case 'u':
                  pstat = num_descriptors;
                  break;
               case 'U':
                  pstat = sysdata.maxplayers;
                  break;
               case 'v':
                  pstat = ch->move;
                  break;
               case 'V':
                  pstat = ch->max_move;
                  break;
               case 'g':
                  pstat = ch->gold;
                  break;
               case 'r':
                  if( IS_IMMORTAL( och ) )
                     pstat = ch->in_room->vnum;
                  break;
               case 'R':
                  if( IS_SET( och->act, PLR_ROOMVNUM ) )
                     sprintf( pbuf, "<#%d> ", ch->in_room->vnum );
                  break;
               case 'i':
                  if( ( !IS_NPC( ch ) && IS_SET( ch->act, PLR_WIZINVIS ) ) ||
                      ( IS_NPC( ch ) && IS_SET( ch->act, ACT_MOBINVIS ) ) )
                     sprintf( pbuf, "(Invis %d) ", ( IS_NPC( ch ) ? ch->mobinvis : ch->pcdata->wizinvis ) );
                  else if( IS_AFFECTED( ch, AFF_INVISIBLE ) )
                     sprintf( pbuf, "(Invis) " );
                  break;
               case 'I':
                  pstat = ( IS_NPC( ch ) ? ( IS_SET( ch->act, ACT_MOBINVIS ) ? ch->mobinvis : 0 )
                            : ( IS_SET( ch->act, PLR_WIZINVIS ) ? ch->pcdata->wizinvis : 0 ) );
                  break;
            }
            if( pstat != 0x80000000 )
               sprintf( pbuf, "%d", pstat );
            pbuf += strlen( pbuf );
            break;
      }
   }
   *pbuf = '\0';
   send_to_char( buf, ch );
   return;
}

void set_pager_input( DESCRIPTOR_DATA * d, char *argument )
{
   while( isspace( *argument ) )
      argument++;
   d->pagecmd = *argument;
   return;
}

bool pager_output( DESCRIPTOR_DATA * d )
{
   register char *last;
   CHAR_DATA *ch;
   int pclines;
   register int lines;
   bool ret;

   if( !d || !d->pagepoint || d->pagecmd == -1 )
      return TRUE;
   ch = d->original ? d->original : d->character;
   pclines = UMAX( ch->pcdata->pagerlen, 5 ) - 1;
   switch ( LOWER( d->pagecmd ) )
   {
      default:
         lines = 0;
         break;
      case 'b':
         lines = -1 - ( pclines * 2 );
         break;
      case 'r':
         lines = -1 - pclines;
         break;
      case 'q':
         d->pagetop = 0;
         d->pagepoint = NULL;
         flush_buffer( d, TRUE );
         DISPOSE( d->pagebuf );
         d->pagesize = MAX_STRING_LENGTH;
         return TRUE;
   }
   while( lines < 0 && d->pagepoint >= d->pagebuf )
      if( *( --d->pagepoint ) == '\n' )
         ++lines;
   if( *d->pagepoint == '\n' && *( ++d->pagepoint ) == '\r' )
      ++d->pagepoint;
   if( d->pagepoint < d->pagebuf )
      d->pagepoint = d->pagebuf;
   for( lines = 0, last = d->pagepoint; lines < pclines; ++last )
      if( !*last )
         break;
      else if( *last == '\n' )
         ++lines;
   if( *last == '\r' )
      ++last;
   if( last != d->pagepoint )
   {
      if( !write_to_descriptor( d, d->pagepoint, ( last - d->pagepoint ) ) )
         return FALSE;
      d->pagepoint = last;
   }
   while( isspace( *last ) )
      ++last;
   if( !*last )
   {
      d->pagetop = 0;
      d->pagepoint = NULL;
      flush_buffer( d, TRUE );
      DISPOSE( d->pagebuf );
      d->pagesize = MAX_STRING_LENGTH;
      return TRUE;
   }
   d->pagecmd = -1;
   if( IS_SET( ch->act, PLR_ANSI ) )
      if( write_to_descriptor( d, ANSI_LBLUE, 0 ) == FALSE )
         return FALSE;
   if( ( ret = write_to_descriptor( d, "(C)ontinue, (R)efresh, (B)ack, (Q)uit: [C] ", 0 ) ) == FALSE )
      return FALSE;
   if( IS_SET( ch->act, PLR_ANSI ) )
   {
      char buf[32];

      sprintf( buf, "%s", color_str( d->pagecolor, ch ) );
      ret = write_to_descriptor( d, buf, 0 );
   }
   return ret;
}
