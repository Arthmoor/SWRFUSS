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
*			   Player movement module			   *
****************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "mud.h"

const short movement_loss[SECT_MAX] = {
   1, 2, 2, 3, 4, 6, 4, 1, 6, 10, 6, 5, 7, 4
};

const char *const dir_name[] = {
   "north", "east", "south", "west", "up", "down",
   "northeast", "northwest", "southeast", "southwest", "somewhere"
};

const int trap_door[] = {
   TRAP_N, TRAP_E, TRAP_S, TRAP_W, TRAP_U, TRAP_D,
   TRAP_NE, TRAP_NW, TRAP_SE, TRAP_SW
};

const short rev_dir[] = {
   2, 3, 0, 1, 5, 4, 9, 8, 7, 6, 10
};

ROOM_INDEX_DATA *vroom_hash[64];

/*
 * Local functions.
 */
bool has_key( CHAR_DATA * ch, int key );

const char *const sect_names[SECT_MAX][2] = {
   {"In a room", "inside"}, {"A City Street", "cities"},
   {"In a field", "fields"}, {"In a forest", "forests"},
   {"hill", "hills"}, {"On a mountain", "mountains"},
   {"In the water", "waters"}, {"In rough water", "waters"},
   {"Underwater", "underwaters"}, {"In the air", "air"},
   {"In a desert", "deserts"}, {"Somewhere", "unknown"},
   {"ocean floor", "ocean floor"}, {"underground", "underground"}
};

const int sent_total[SECT_MAX] = {
   4, 24, 4, 4, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1
};

const char *const room_sents[SECT_MAX][25] = {
   {
    "The rough hewn walls are made of granite.",
    "You see an occasional spider crawling around.",
    "You notice signs of a recent battle from the bloodstains on the floor.",
    "This place hasa damp musty odour not unlike rotting vegetation."},

   {
    "You notice the occasional stray looking for food.",
    "Tall buildings loom on either side of you stretching to the sky.",
    "Some street people are putting on an interesting display of talent trying to earn some credits.",
    "Two people nearby shout heated words of argument at one another.",
    "You think you can make out several shady figures talking down a dark alleyway."
    "A slight breeze blows through the tall buildings.",
    "A small crowd of people have gathered at one side of the street.",
    "Clouds far above you obscure the tops of the highest skyscrapers.",
    "A speeder moves slowly through the street avoiding pedestrians.",
    "A cloudcar flys by overhead.",
    "The air is thick and hard to breath.",
    "The many smells of the city assault your senses.",
    "You hear a scream far of in the distance.",
    "The buildings around you seem endless in number.",
    "The city stretches seemingly endless in all directions.",
    "The street is wide and long.",
    "A swoop rider passes quickly by weaving in and out of pedestrians and other vehicles.",
    "The surface of the road is worn from many travellers.",
    "You feel it would be very easy to get lost in such an enormous city.",
    "You can see other streets above and bellow this one running in many directions.",
    "There are entrances to several buildings at this level.",
    "Along the edge of the street railings prevent pedestrians from falling to their death.",
    "In between the many towers you can see down into depths of the lower city.",
    "A grate in the street prevents rainwater from building up.",
    "You can see you reflection in several of the transparisteel windows as you pass by."
    "You hear a scream far of in the distance.",
    },

   {
    "You notice sparce patches of brush and shrubs.",
    "There is a small cluster of trees far off in the distance.",
    "Around you are grassy fields as far as the eye can see.",
    "Throughout the plains a wide variety of weeds and wildflowers are scattered."},

   {
    "Tall, dark evergreens prevent you from seeing very far.",
    "Many huge oak trees that look several hundred years old are here.",
    "You notice a solitary lonely weeping willow.",
    "To your left is a patch of bright white birch trees, slender and tall."},

   {
    "The rolling hills are lightly speckled with violet wildflowers."},

   {
    "The rocky mountain pass offers many hiding places."},

   {
    "The water is smooth as glass."},

   {
    "Rough waves splash about angrily."},

   {
    "A small school of fish swims by."},

   {
    "The land is far far below.",
    "A misty haze of clouds drifts by."},

   {
    "Around you is sand as far as the eye can see.",
    "You think you see an oasis far in the distance."},

   {
    "You notice nothing unusual."},

   {
    "There are many rocks and coral which litter the ocean floor."},

   {
    "You stand in a lengthy tunnel of rock."}
};

int wherehome( CHAR_DATA * ch )
{
   if( ch->plr_home )
      return ch->plr_home->vnum;

   if( get_trust( ch ) >= LEVEL_IMMORTAL )
      return ROOM_START_IMMORTAL;
   if( ch->race == RACE_HUMAN )
      return ROOM_START_HUMAN;
   if( ch->race == RACE_WOOKIEE )
      return ROOM_START_WOOKIEE;
   if( ch->race == RACE_RODIAN )
      return ROOM_START_RODIAN;
   if( ch->race == RACE_MON_CALAMARI )
      return ROOM_START_MON_CALAMARIAN;
   if( ch->race == RACE_TWI_LEK )
      return ROOM_START_TWILEK;
   if( ch->race == RACE_HUTT )
      return ROOM_START_HUTT;
   if( ch->race == RACE_GAMORREAN )
      return ROOM_START_GAMORREAN;
   if( ch->race == RACE_JAWA )
      return ROOM_START_JAWA;
   if( ch->race == RACE_ADARIAN )
      return ROOM_START_ADARIAN;
   if( ch->race == RACE_EWOK )
      return ROOM_START_EWOK;
   if( ch->race == RACE_VERPINE )
      return ROOM_START_VERPINE;
   if( ch->race == RACE_DEFEL )
      return ROOM_START_DEFEL;
   if( ch->race == RACE_TRANDOSHAN )
      return ROOM_START_TRANDOSHAN;
   if( ch->race == RACE_CHADRA_FAN )
      return ROOM_START_CHADRA_FAN;
   if( ch->race == RACE_QUARREN )
      return ROOM_START_QUARREN;
   if( ch->race == RACE_DUINUOGWUIN )
      return ROOM_START_DUINUOGWUIN;
   if( ch->race == RACE_NOGHRI )
      return ROOM_START_NOGHRI;

   return ROOM_VNUM_TEMPLE;
}

char *grab_word( char *argument, char *arg_first )
{
   char cEnd;
   short count;

   count = 0;

   while( isspace( *argument ) )
      argument++;

   cEnd = ' ';
   if( *argument == '\'' || *argument == '"' )
      cEnd = *argument++;

   while( *argument != '\0' || ++count >= 255 )
   {
      if( *argument == cEnd )
      {
         argument++;
         break;
      }
      *arg_first++ = *argument++;
   }
   *arg_first = '\0';

   while( isspace( *argument ) )
      argument++;

   return argument;
}

char *wordwrap( char *txt, short wrap )
{
   static char buf[MAX_STRING_LENGTH];
   char *bufp;

   buf[0] = '\0';
   bufp = buf;
   if( txt != NULL )
   {
      char line[MAX_INPUT_LENGTH];
      char temp[MAX_INPUT_LENGTH];
      char *ptr, *p;
      int ln, x;

      ++bufp;
      line[0] = '\0';
      ptr = txt;
      while( *ptr )
      {
         ptr = grab_word( ptr, temp );
         ln = strlen( line );
         x = strlen( temp );
         if( ( ln + x + 1 ) < wrap )
         {
            if( line[ln - 1] == '.' )
               strlcat( line, "  ", MAX_INPUT_LENGTH );
            else
               strlcat( line, " ", MAX_INPUT_LENGTH );
            strlcat( line, temp, MAX_INPUT_LENGTH );
            p = strchr( line, '\n' );
            if( !p )
               p = strchr( line, '\r' );
            if( p )
            {
               strlcat( buf, line, MAX_STRING_LENGTH );
               line[0] = '\0';
            }
         }
         else
         {
            strlcat( line, "\r\n", MAX_INPUT_LENGTH );
            strlcat( buf, line, MAX_STRING_LENGTH );
            strlcpy( line, temp, MAX_INPUT_LENGTH );
         }
      }
      if( line[0] != '\0' )
         strlcat( buf, line, MAX_STRING_LENGTH );
   }
   return bufp;
}

void decorate_room( ROOM_INDEX_DATA * room )
{
   char buf[MAX_STRING_LENGTH];
   char buf2[MAX_STRING_LENGTH];
   int nRand;
   int iRand, len;
   int previous[8];
   int sector = room->sector_type;

   if( room->name )
      STRFREE( room->name );
   if( room->description )
      STRFREE( room->description );

   room->name = STRALLOC( sect_names[sector][0] );
   buf[0] = '\0';
   nRand = number_range( 1, UMIN( 8, sent_total[sector] ) );

   for( iRand = 0; iRand < nRand; iRand++ )
      previous[iRand] = -1;

   for( iRand = 0; iRand < nRand; iRand++ )
   {
      while( previous[iRand] == -1 )
      {
         int x, z;

         x = number_range( 0, sent_total[sector] - 1 );

         for( z = 0; z < iRand; z++ )
            if( previous[z] == x )
               break;

         if( z < iRand )
            continue;

         previous[iRand] = x;

         len = strlen( buf );
         snprintf( buf2, MAX_STRING_LENGTH, "%s", room_sents[sector][x] );
         if( len > 5 && buf[len - 1] == '.' )
         {
            strlcat( buf, "  ", MAX_STRING_LENGTH );
            buf2[0] = UPPER( buf2[0] );
         }
         else if( len == 0 )
            buf2[0] = UPPER( buf2[0] );
         strlcat( buf, buf2, MAX_STRING_LENGTH );
      }
   }
   snprintf( buf2, MAX_STRING_LENGTH, "%s\r\n", wordwrap( buf, 78 ) );
   room->description = STRALLOC( buf2 );
}

/*
 * Remove any unused virtual rooms				-Thoric
 */
void clear_vrooms(  )
{
   int hash;
   ROOM_INDEX_DATA *room, *room_next, *prev;

   for( hash = 0; hash < 64; hash++ )
   {
      while( vroom_hash[hash] && !vroom_hash[hash]->first_person && !vroom_hash[hash]->first_content )
      {
         room = vroom_hash[hash];
         vroom_hash[hash] = room->next;
         clean_room( room );
         DISPOSE( room );
         --top_vroom;
      }
      prev = NULL;
      for( room = vroom_hash[hash]; room; room = room_next )
      {
         room_next = room->next;
         if( !room->first_person && !room->first_content )
         {
            if( prev )
               prev->next = room_next;
            clean_room( room );
            DISPOSE( room );
            --top_vroom;
         }
         if( room )
            prev = room;
      }
   }
}

/*
 * Function to get the equivelant exit of DIR 0-MAXDIR out of linked list.
 * Made to allow old-style diku-merc exit functions to work.	-Thoric
 */
EXIT_DATA *get_exit( ROOM_INDEX_DATA * room, short dir )
{
   EXIT_DATA *xit;

   if( !room )
   {
      bug( "%s: NULL room", __func__ );
      return NULL;
   }

   for( xit = room->first_exit; xit; xit = xit->next )
      if( xit->vdir == dir )
         return xit;
   return NULL;
}

/*
 * Function to get an exit, leading the the specified room
 */
EXIT_DATA *get_exit_to( ROOM_INDEX_DATA * room, short dir, int vnum )
{
   EXIT_DATA *xit;

   if( !room )
   {
      bug( "%s: NULL room", __func__ );
      return NULL;
   }

   for( xit = room->first_exit; xit; xit = xit->next )
      if( xit->vdir == dir && xit->vnum == vnum )
         return xit;
   return NULL;
}

/*
 * Function to get the nth exit of a room			-Thoric
 */
EXIT_DATA *get_exit_num( ROOM_INDEX_DATA * room, short count )
{
   EXIT_DATA *xit;
   int cnt;

   if( !room )
   {
      bug( "%s: NULL room", __func__ );
      return NULL;
   }

   for( cnt = 0, xit = room->first_exit; xit; xit = xit->next )
      if( ++cnt == count )
         return xit;
   return NULL;
}

/*
 * Modify movement due to encumbrance				-Thoric
 */
short encumbrance( CHAR_DATA * ch, short move )
{
  int cur, max;

  max = can_carry_w( ch );
  cur = ch->carry_weight;

  if( cur >= max )
    return move * 4;
  else if( cur >= max * 0.95 )
    return ( short )( move * 3.5 );
  else if( cur >= max * 0.90 )
    return move * 3;
  else if( cur >= max * 0.85 )
    return ( short )( move * 2.5 );
  else if( cur >= max * 0.80 )
    return move * 2;
  else if( cur >= max * 0.75 )
    return ( short )( move * 1.5 );
  else
    return move;
}

/*
 * Check to see if a character can fall down, checks for looping   -Thoric
 */
bool will_fall( CHAR_DATA * ch, int fall )
{
   if( IS_SET( ch->in_room->room_flags, ROOM_NOFLOOR )
       && CAN_GO( ch, DIR_DOWN )
       && ( !IS_AFFECTED( ch, AFF_FLYING ) || ( ch->mount && !IS_AFFECTED( ch->mount, AFF_FLYING ) ) ) )
   {
      if( fall > 80 )
      {
         bug( "%s: Falling (in a loop?) more than 80 rooms: vnum %d", __func__, ch->in_room->vnum );
         char_from_room( ch );
         char_to_room( ch, get_room_index( wherehome( ch ) ) );
         fall = 0;
         return TRUE;
      }
      set_char_color( AT_FALLING, ch );
      send_to_char( "You're falling down...\r\n", ch );
      move_char( ch, get_exit( ch->in_room, DIR_DOWN ), ++fall );
      return TRUE;
   }
   return FALSE;
}

/*
 * create a 'virtual' room					-Thoric
 */
ROOM_INDEX_DATA *generate_exit( ROOM_INDEX_DATA * in_room, EXIT_DATA ** pexit )
{
   EXIT_DATA *xit, *bxit;
   EXIT_DATA *orig_exit = ( EXIT_DATA * ) * pexit;
   ROOM_INDEX_DATA *room, *backroom;
   int brvnum;
   int serial;
   int roomnum;
   int distance = -1;
   int vdir = orig_exit->vdir;
   short hash;
   bool found = FALSE;

   if( in_room->vnum > 32767 )   /* room is virtual */
   {
      serial = in_room->vnum;
      roomnum = in_room->tele_vnum;
      if( ( serial & 65535 ) == orig_exit->vnum )
      {
         brvnum = serial >> 16;
         --roomnum;
         distance = roomnum;
      }
      else
      {
         brvnum = serial & 65535;
         ++roomnum;
         distance = orig_exit->distance - 1;
      }
      backroom = get_room_index( brvnum );
   }
   else
   {
      int r1 = in_room->vnum;
      int r2 = orig_exit->vnum;

      brvnum = r1;
      backroom = in_room;
      serial = ( UMAX( r1, r2 ) << 16 ) | UMIN( r1, r2 );
      distance = orig_exit->distance - 1;
      roomnum = r1 < r2 ? 1 : distance;
   }
   hash = serial % 64;

   for( room = vroom_hash[hash]; room; room = room->next )
      if( room->vnum == serial && room->tele_vnum == roomnum )
      {
         found = TRUE;
         break;
      }
   if( !found )
   {
      CREATE( room, ROOM_INDEX_DATA, 1 );
      room->area = in_room->area;
      room->vnum = serial;
      room->tele_vnum = roomnum;
      room->sector_type = in_room->sector_type;
      room->room_flags = in_room->room_flags;
      decorate_room( room );
      room->next = vroom_hash[hash];
      vroom_hash[hash] = room;
      ++top_vroom;
   }
   if( !found || ( xit = get_exit( room, vdir ) ) == NULL )
   {
      xit = make_exit( room, orig_exit->to_room, vdir );
      xit->keyword = STRALLOC( "" );
      xit->description = STRALLOC( "" );
      xit->key = -1;
      xit->distance = distance;
   }
   if( !found )
   {
      bxit = make_exit( room, backroom, rev_dir[vdir] );
      bxit->keyword = STRALLOC( "" );
      bxit->description = STRALLOC( "" );
      bxit->key = -1;
      if( ( serial & 65535 ) != orig_exit->vnum )
         bxit->distance = roomnum;
      else
      {
         EXIT_DATA *tmp = get_exit( backroom, vdir );
         int fulldist = tmp->distance;

         bxit->distance = fulldist - distance;
      }
   }
   *pexit = xit;
   return room;
}

ch_ret move_char( CHAR_DATA * ch, EXIT_DATA * pexit, int fall )
{
   ROOM_INDEX_DATA *in_room, *to_room, *from_room;
   char buf[MAX_STRING_LENGTH];
   const char *txt, *dtxt;
   ch_ret retcode;
   short door, distance;
   bool drunk = FALSE, brief = FALSE;

   if( !IS_NPC( ch ) )
      if( IS_DRUNK( ch, 2 ) && ( ch->position != POS_SHOVE ) && ( ch->position != POS_DRAG ) )
         drunk = TRUE;

   if( drunk && !fall )
   {
      door = number_door(  );
      pexit = get_exit( ch->in_room, door );
   }

#ifdef DEBUG
   if( pexit )
      bug( "%s: %s to door %d", __func__, ch->name, pexit->vdir );
#endif

   retcode = rNONE;
   txt = NULL;

   if( IS_NPC( ch ) && IS_SET( ch->act, ACT_MOUNTED ) )
      return retcode;

   in_room = ch->in_room;
   from_room = in_room;
   if( !pexit || ( to_room = pexit->to_room ) == NULL )
   {
      if( drunk )
         send_to_char( "You hit a wall in your drunken state.\r\n", ch );
      else
         send_to_char( "Alas, you cannot go that way.\r\n", ch );
      return rNONE;
   }

   door = pexit->vdir;
   distance = pexit->distance;

   /*
    * Exit is only a "window", there is no way to travel in that direction
    * unless it's a door with a window in it      -Thoric
    */
   if( IS_SET( pexit->exit_info, EX_WINDOW ) && !IS_SET( pexit->exit_info, EX_ISDOOR ) )
   {
      send_to_char( "Alas, you cannot go that way.\r\n", ch );
      return rNONE;
   }

   if( IS_SET( pexit->exit_info, EX_PORTAL ) && IS_NPC( ch ) )
   {
      act( AT_PLAIN, "Mobs can't use portals.", ch, NULL, NULL, TO_CHAR );
      return rNONE;
   }

   if( IS_SET( pexit->exit_info, EX_NOMOB ) && IS_NPC( ch ) )
   {
      act( AT_PLAIN, "Mobs can't enter there.", ch, NULL, NULL, TO_CHAR );
      return rNONE;
   }

   if( IS_SET( to_room->room_flags, ROOM_NO_MOB ) && IS_NPC( ch ) )
   {
      act( AT_PLAIN, "Mobs can't enter there.", ch, NULL, NULL, TO_CHAR );
      return rNONE;
   }

   if( IS_SET( pexit->exit_info, EX_CLOSED )
       && ( !IS_AFFECTED( ch, AFF_PASS_DOOR ) || IS_SET( pexit->exit_info, EX_NOPASSDOOR ) ) )
   {
      if( !IS_SET( pexit->exit_info, EX_SECRET ) && !IS_SET( pexit->exit_info, EX_DIG ) )
      {
         if( drunk )
         {
            act( AT_PLAIN, "$n runs into the $d in $s drunken state.", ch, NULL, pexit->keyword, TO_ROOM );
            act( AT_PLAIN, "You run into the $d in your drunken state.", ch, NULL, pexit->keyword, TO_CHAR );
         }
         else
            act( AT_PLAIN, "The $d is closed.", ch, NULL, pexit->keyword, TO_CHAR );
      }
      else
      {
         if( drunk )
            send_to_char( "You hit a wall in your drunken state.\r\n", ch );
         else
            send_to_char( "Alas, you cannot go that way.\r\n", ch );
      }
      return rNONE;
   }

   /*
    * Crazy virtual room idea, created upon demand.     -Thoric
    */
   if( distance > 1 )
      if( ( to_room = generate_exit( in_room, &pexit ) ) == NULL )
         send_to_char( "Alas, you cannot go that way.\r\n", ch );

   if( !fall && IS_AFFECTED( ch, AFF_CHARM ) && ch->master && in_room == ch->master->in_room )
   {
      send_to_char( "What?  And leave your beloved master?\r\n", ch );
      return rNONE;
   }

   if( room_is_private( ch, to_room ) )
   {
      send_to_char( "That room is private right now.\r\n", ch );
      return rNONE;
   }

   if( !IS_IMMORTAL( ch ) && !IS_NPC( ch ) && ch->in_room->area != to_room->area )
   {
      if( ch->top_level < to_room->area->low_hard_range )
      {
         set_char_color( AT_TELL, ch );
         switch ( to_room->area->low_hard_range - ch->top_level )
         {
            case 1:
               send_to_char( "A voice in your mind says, 'You are nearly ready to go that way...'", ch );
               break;
            case 2:
               send_to_char( "A voice in your mind says, 'Soon you shall be ready to travel down this path... soon.'", ch );
               break;
            case 3:
               send_to_char( "A voice in your mind says, 'You are not ready to go down that path... yet.'.\r\n", ch );
               break;
            default:
               send_to_char( "A voice in your mind says, 'You are not ready to go down that path.'.\r\n", ch );
         }
         return rNONE;
      }
      else if( ch->top_level > to_room->area->hi_hard_range )
      {
         set_char_color( AT_TELL, ch );
         send_to_char( "A voice in your mind says, 'There is nothing more for you down that path.'", ch );
         return rNONE;
      }
   }

   if( !fall && !IS_NPC( ch ) )
   {
      /*
       * int iClass;
       */
      int move;

      if( in_room->sector_type == SECT_AIR || to_room->sector_type == SECT_AIR || IS_SET( pexit->exit_info, EX_FLY ) )
      {
         if( ch->mount && !IS_AFFECTED( ch->mount, AFF_FLYING ) )
         {
            send_to_char( "Your mount can't fly.\r\n", ch );
            return rNONE;
         }
         if( !ch->mount && !IS_AFFECTED( ch, AFF_FLYING ) )
         {
            send_to_char( "You'd need to fly to go there.\r\n", ch );
            return rNONE;
         }
      }

      if( in_room->sector_type == SECT_WATER_NOSWIM || to_room->sector_type == SECT_WATER_NOSWIM )
      {
         OBJ_DATA *obj;
         bool found;

         found = FALSE;
         if( ch->mount )
         {
            if( IS_AFFECTED( ch->mount, AFF_FLYING ) || IS_AFFECTED( ch->mount, AFF_FLOATING ) )
               found = TRUE;
         }
         else if( IS_AFFECTED( ch, AFF_FLYING ) || IS_AFFECTED( ch, AFF_FLOATING ) )
            found = TRUE;

         /*
          * Look for a boat.
          */
         if( !found )
            for( obj = ch->first_carrying; obj; obj = obj->next_content )
            {
               if( obj->item_type == ITEM_BOAT )
               {
                  found = TRUE;
                  if( drunk )
                     txt = "paddles unevenly";
                  else
                     txt = "paddles";
                  break;
               }
            }

         if( !found )
         {
            send_to_char( "You'd need a boat to go there.\r\n", ch );
            return rNONE;
         }
      }

      if( IS_SET( pexit->exit_info, EX_CLIMB ) )
      {
         bool found;

         found = FALSE;
         if( ch->mount && IS_AFFECTED( ch->mount, AFF_FLYING ) )
            found = TRUE;
         else if( IS_AFFECTED( ch, AFF_FLYING ) )
            found = TRUE;

         if( !found && !ch->mount )
         {
            if( ( !IS_NPC( ch ) && number_percent(  ) > ch->pcdata->learned[gsn_climb] ) || drunk || ch->mental_state < -90 )
            {
               send_to_char( "You start to climb... but lose your grip and fall!\r\n", ch );
               learn_from_failure( ch, gsn_climb );
               if( pexit->vdir == DIR_DOWN )
               {
                  retcode = move_char( ch, pexit, 1 );
                  return retcode;
               }
               set_char_color( AT_HURT, ch );
               send_to_char( "OUCH! You hit the ground!\r\n", ch );
               WAIT_STATE( ch, 20 );
               retcode = damage( ch, ch, ( pexit->vdir == DIR_UP ? 10 : 5 ), TYPE_UNDEFINED );
               return retcode;
            }
            found = TRUE;
            learn_from_success( ch, gsn_climb );
            WAIT_STATE( ch, skill_table[gsn_climb]->beats );
            txt = "climbs";
         }

         if( !found )
         {
            send_to_char( "You can't climb.\r\n", ch );
            return rNONE;
         }
      }

      if( ch->mount )
      {
         switch ( ch->mount->position )
         {
            case POS_DEAD:
               send_to_char( "Your mount is dead!\r\n", ch );
               return rNONE;
               break;

            case POS_MORTAL:
            case POS_INCAP:
               send_to_char( "Your mount is hurt far too badly to move.\r\n", ch );
               return rNONE;
               break;

            case POS_STUNNED:
               send_to_char( "Your mount is too stunned to do that.\r\n", ch );
               return rNONE;
               break;

            case POS_SLEEPING:
               send_to_char( "Your mount is sleeping.\r\n", ch );
               return rNONE;
               break;

            case POS_RESTING:
               send_to_char( "Your mount is resting.\r\n", ch );
               return rNONE;
               break;

            case POS_SITTING:
               send_to_char( "Your mount is sitting down.\r\n", ch );
               return rNONE;
               break;

            default:
               break;
         }

         if( !IS_AFFECTED( ch->mount, AFF_FLYING ) && !IS_AFFECTED( ch->mount, AFF_FLOATING ) )
            move = movement_loss[UMIN( SECT_MAX - 1, in_room->sector_type )];
         else
            move = 1;
         if( ch->mount->move < move )
         {
            send_to_char( "Your mount is too exhausted.\r\n", ch );
            return rNONE;
         }
      }
      else
      {
         if( !IS_AFFECTED( ch, AFF_FLYING ) && !IS_AFFECTED( ch, AFF_FLOATING ) )
            move = encumbrance( ch, movement_loss[UMIN( SECT_MAX - 1, in_room->sector_type )] );
         else
            move = 1;
         if( ch->move < move )
         {
            send_to_char( "You are too exhausted.\r\n", ch );
            return rNONE;
         }
      }

      WAIT_STATE( ch, move );
      if( ch->mount )
         ch->mount->move -= move;
      else
         ch->move -= move;
   }

   /*
    * Check if player can fit in the room
    */
   if( to_room->tunnel > 0 )
   {
      CHAR_DATA *ctmp;
      int count = ch->mount ? 1 : 0;

      for( ctmp = to_room->first_person; ctmp; ctmp = ctmp->next_in_room )
         if( ++count >= to_room->tunnel )
         {
            if( ch->mount && count == to_room->tunnel )
               send_to_char( "There is no room for both you and your mount in there.\r\n", ch );
            else
               send_to_char( "There is no room for you in there.\r\n", ch );
            return rNONE;
         }
   }

   /*
    * check for traps on exit - later 
    */

   if( !IS_AFFECTED( ch, AFF_SNEAK ) && ( IS_NPC( ch ) || !IS_SET( ch->act, PLR_WIZINVIS ) ) )
   {
      if( fall )
         txt = "falls";
      else if( !txt )
      {
         if( ch->mount )
         {
            if( IS_AFFECTED( ch->mount, AFF_FLOATING ) )
               txt = "floats";
            else if( IS_AFFECTED( ch->mount, AFF_FLYING ) )
               txt = "flys";
            else
               txt = "rides";
         }
         else
         {
            if( IS_AFFECTED( ch, AFF_FLOATING ) )
            {
               if( drunk )
                  txt = "floats unsteadily";
               else
                  txt = "floats";
            }
            else if( IS_AFFECTED( ch, AFF_FLYING ) )
            {
               if( drunk )
                  txt = "flys shakily";
               else
                  txt = "flys";
            }
            else if( ch->position == POS_SHOVE )
               txt = "is shoved";
            else if( ch->position == POS_DRAG )
               txt = "is dragged";
            else
            {
               if( drunk )
                  txt = "stumbles drunkenly";
               else
                  txt = "leaves";
            }
         }
      }
      if( ch->mount )
      {
         snprintf( buf, MAX_STRING_LENGTH, "$n %s %s upon $N.", txt, dir_name[door] );
         act( AT_ACTION, buf, ch, NULL, ch->mount, TO_NOTVICT );
      }
      else
      {
         snprintf( buf, MAX_STRING_LENGTH, "$n %s $T.", txt );
         act( AT_ACTION, buf, ch, NULL, dir_name[door], TO_ROOM );
      }
   }

   rprog_leave_trigger( ch );
   if( char_died( ch ) )
      return global_retcode;

   char_from_room( ch );
   char_to_room( ch, to_room );
   if( ch->mount )
   {
      rprog_leave_trigger( ch->mount );

      /*
       * Mount bug fix test. -Orion
       */
      if( char_died( ch->mount ) )
         return global_retcode;

      if( ch->mount )
      {
         char_from_room( ch->mount );
         char_to_room( ch->mount, to_room );
      }
   }

   if( !IS_AFFECTED( ch, AFF_SNEAK ) && ( IS_NPC( ch ) || !IS_SET( ch->act, PLR_WIZINVIS ) ) )
   {
      if( fall )
         txt = "falls";
      else if( ch->mount )
      {
         if( IS_AFFECTED( ch->mount, AFF_FLOATING ) )
            txt = "floats in";
         else if( IS_AFFECTED( ch->mount, AFF_FLYING ) )
            txt = "flys in";
         else
            txt = "rides in";
      }
      else
      {
         if( IS_AFFECTED( ch, AFF_FLOATING ) )
         {
            if( drunk )
               txt = "floats in unsteadily";
            else
               txt = "floats in";
         }
         else if( IS_AFFECTED( ch, AFF_FLYING ) )
         {
            if( drunk )
               txt = "flys in shakily";
            else
               txt = "flys in";
         }
         else if( ch->position == POS_SHOVE )
            txt = "is shoved in";
         else if( ch->position == POS_DRAG )
            txt = "is dragged in";
         else
         {
            if( drunk )
               txt = "stumbles drunkenly in";
            else
               txt = "arrives";
         }
      }
      switch ( door )
      {
         default:
            dtxt = "somewhere";
            break;
         case 0:
            dtxt = "the south";
            break;
         case 1:
            dtxt = "the west";
            break;
         case 2:
            dtxt = "the north";
            break;
         case 3:
            dtxt = "the east";
            break;
         case 4:
            dtxt = "below";
            break;
         case 5:
            dtxt = "above";
            break;
         case 6:
            dtxt = "the south-west";
            break;
         case 7:
            dtxt = "the south-east";
            break;
         case 8:
            dtxt = "the north-west";
            break;
         case 9:
            dtxt = "the north-east";
            break;
      }
      if( ch->mount )
      {
         snprintf( buf, MAX_STRING_LENGTH, "$n %s from %s upon $N.", txt, dtxt );
         act( AT_ACTION, buf, ch, NULL, ch->mount, TO_ROOM );
      }
      else
      {
         snprintf( buf, MAX_STRING_LENGTH, "$n %s from %s.", txt, dtxt );
         act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );
      }
   }

   if( !IS_IMMORTAL( ch ) && !IS_NPC( ch ) && ch->in_room->area != to_room->area )
   {
      if( ch->top_level < to_room->area->low_soft_range )
      {
         set_char_color( AT_MAGIC, ch );
         send_to_char( "You feel uncomfortable being in this strange land...\r\n", ch );
      }
      else if( ch->top_level > to_room->area->hi_soft_range )
      {
         set_char_color( AT_MAGIC, ch );
         send_to_char( "You feel there is not much to gain visiting this place...\r\n", ch );
      }
   }

   do_look( ch, "auto" );
   if( brief )
      SET_BIT( ch->act, PLR_BRIEF );

   /*
    * BIG ugly looping problem here when the character is mptransed back
    * to the starting room.  To avoid this, check how many chars are in 
    * the room at the start and stop processing followers after doing
    * the right number of them.  -- Narn
    */
   if( !fall )
   {
      CHAR_DATA *fch;
      CHAR_DATA *nextinroom;
      int chars = 0, count = 0;

      for( fch = from_room->first_person; fch; fch = fch->next_in_room )
         chars++;

      for( fch = from_room->first_person; fch && ( count < chars ); fch = nextinroom )
      {
         nextinroom = fch->next_in_room;
         count++;
         if( fch != ch  /* loop room bug fix here by Thoric */
             && fch->master == ch && fch->position == POS_STANDING )
         {
            if( !get_exit( from_room, door ) )
            {
              act( AT_ACTION, "The entrance closes behind $N, preventing you from following!", fch, NULL, ch, TO_CHAR );
              continue;
            }
            act( AT_ACTION, "You follow $N.", fch, NULL, ch, TO_CHAR );
            move_char( fch, pexit, 0 );
         }
      }
   }

   if( ch->in_room->first_content )
      retcode = check_room_for_traps( ch, TRAP_ENTER_ROOM );
   if( retcode != rNONE )
      return retcode;

   if( char_died( ch ) )
      return retcode;

   mprog_entry_trigger( ch );
   if( char_died( ch ) )
      return retcode;

   rprog_enter_trigger( ch );
   if( char_died( ch ) )
      return retcode;

   mprog_greet_trigger( ch );
   if( char_died( ch ) )
      return retcode;

   oprog_greet_trigger( ch );
   if( char_died( ch ) )
      return retcode;

   if( !will_fall( ch, fall ) && fall > 0 )
   {
      if( !IS_AFFECTED( ch, AFF_FLOATING ) || ( ch->mount && !IS_AFFECTED( ch->mount, AFF_FLOATING ) ) )
      {
         set_char_color( AT_HURT, ch );
         send_to_char( "OUCH! You hit the ground!\r\n", ch );
         WAIT_STATE( ch, 20 );
         retcode = damage( ch, ch, 50 * fall, TYPE_UNDEFINED );
      }
      else
      {
         set_char_color( AT_MAGIC, ch );
         send_to_char( "You lightly float down to the ground.\r\n", ch );
      }
   }
   return retcode;
}

void do_north( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_NORTH ), 0 );
}

void do_east( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_EAST ), 0 );
}

void do_south( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_SOUTH ), 0 );
}

void do_west( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_WEST ), 0 );
}

void do_up( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_UP ), 0 );
}

void do_down( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_DOWN ), 0 );
}

void do_northeast( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_NORTHEAST ), 0 );
}

void do_northwest( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_NORTHWEST ), 0 );
}

void do_southeast( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_SOUTHEAST ), 0 );
}

void do_southwest( CHAR_DATA * ch, const char *argument )
{
   move_char( ch, get_exit( ch->in_room, DIR_SOUTHWEST ), 0 );
}

EXIT_DATA *find_door( CHAR_DATA * ch, const char *arg, bool quiet )
{
   EXIT_DATA *pexit;
   int door;

   if( arg == NULL || !str_cmp( arg, "" ) )
      return NULL;

   pexit = NULL;
   if( !str_cmp( arg, "n" ) || !str_cmp( arg, "north" ) )
      door = 0;
   else if( !str_cmp( arg, "e" ) || !str_cmp( arg, "east" ) )
      door = 1;
   else if( !str_cmp( arg, "s" ) || !str_cmp( arg, "south" ) )
      door = 2;
   else if( !str_cmp( arg, "w" ) || !str_cmp( arg, "west" ) )
      door = 3;
   else if( !str_cmp( arg, "u" ) || !str_cmp( arg, "up" ) )
      door = 4;
   else if( !str_cmp( arg, "d" ) || !str_cmp( arg, "down" ) )
      door = 5;
   else if( !str_cmp( arg, "ne" ) || !str_cmp( arg, "northeast" ) )
      door = 6;
   else if( !str_cmp( arg, "nw" ) || !str_cmp( arg, "northwest" ) )
      door = 7;
   else if( !str_cmp( arg, "se" ) || !str_cmp( arg, "southeast" ) )
      door = 8;
   else if( !str_cmp( arg, "sw" ) || !str_cmp( arg, "southwest" ) )
      door = 9;
   else
   {
      for( pexit = ch->in_room->first_exit; pexit; pexit = pexit->next )
      {
         if( ( quiet || IS_SET( pexit->exit_info, EX_ISDOOR ) ) && pexit->keyword && nifty_is_name( arg, pexit->keyword ) )
            return pexit;
      }
      if( !quiet )
         act( AT_PLAIN, "You see no $T here.", ch, NULL, arg, TO_CHAR );
      return NULL;
   }

   if( ( pexit = get_exit( ch->in_room, door ) ) == NULL )
   {
      if( !quiet )
         act( AT_PLAIN, "You see no $T here.", ch, NULL, arg, TO_CHAR );
      return NULL;
   }

   if( quiet )
      return pexit;

   if( IS_SET( pexit->exit_info, EX_SECRET ) )
   {
      act( AT_PLAIN, "You see no $T here.", ch, NULL, arg, TO_CHAR );
      return NULL;
   }

   if( !IS_SET( pexit->exit_info, EX_ISDOOR ) )
   {
      send_to_char( "You can't do that.\r\n", ch );
      return NULL;
   }

   return pexit;
}

void toggle_bexit_flag( EXIT_DATA * pexit, int flag )
{
   EXIT_DATA *pexit_rev;

   TOGGLE_BIT( pexit->exit_info, flag );
   if( ( pexit_rev = pexit->rexit ) != NULL && pexit_rev != pexit )
      TOGGLE_BIT( pexit_rev->exit_info, flag );
}

void set_bexit_flag( EXIT_DATA * pexit, int flag )
{
   EXIT_DATA *pexit_rev;

   SET_BIT( pexit->exit_info, flag );
   if( ( pexit_rev = pexit->rexit ) != NULL && pexit_rev != pexit )
      SET_BIT( pexit_rev->exit_info, flag );
}

void remove_bexit_flag( EXIT_DATA * pexit, int flag )
{
   EXIT_DATA *pexit_rev;

   REMOVE_BIT( pexit->exit_info, flag );
   if( ( pexit_rev = pexit->rexit ) != NULL && pexit_rev != pexit )
      REMOVE_BIT( pexit_rev->exit_info, flag );
}

void do_open( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   EXIT_DATA *pexit;
   int door;

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      do_openhatch( ch, "" );
      return;
   }

   if( ( pexit = find_door( ch, arg, TRUE ) ) != NULL )
   {
      /*
       * 'open door' 
       */
      EXIT_DATA *pexit_rev;

      if( !IS_SET( pexit->exit_info, EX_ISDOOR ) )
      {
         send_to_char( "You can't do that.\r\n", ch );
         return;
      }
      if( !IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "It's already open.\r\n", ch );
         return;
      }
      if( IS_SET( pexit->exit_info, EX_LOCKED ) )
      {
         send_to_char( "It's locked.\r\n", ch );
         return;
      }

      if( !IS_SET( pexit->exit_info, EX_SECRET ) || ( pexit->keyword && nifty_is_name( arg, pexit->keyword ) ) )
      {
         act( AT_ACTION, "$n opens the $d.", ch, NULL, pexit->keyword, TO_ROOM );
         act( AT_ACTION, "You open the $d.", ch, NULL, pexit->keyword, TO_CHAR );
         if( ( pexit_rev = pexit->rexit ) != NULL && pexit_rev->to_room == ch->in_room )
         {
            CHAR_DATA *rch;

            for( rch = pexit->to_room->first_person; rch; rch = rch->next_in_room )
               act( AT_ACTION, "The $d opens.", rch, NULL, pexit_rev->keyword, TO_CHAR );
            sound_to_room( pexit->to_room, "!!SOUND(door)" );
         }
         remove_bexit_flag( pexit, EX_CLOSED );
         if( ( door = pexit->vdir ) >= 0 && door < 10 )
            check_room_for_traps( ch, trap_door[door] );

         sound_to_room( ch->in_room, "!!SOUND(door)" );
         return;
      }
   }

   if( ( obj = get_obj_here( ch, arg ) ) != NULL )
   {
      /*
       * 'open object' 
       */
      if( obj->item_type != ITEM_CONTAINER )
      {
         ch_printf( ch, "%s isn't a container.\r\n", capitalize( obj->short_descr ) );
         return;
      }
      if( !IS_SET( obj->value[1], CONT_CLOSED ) )
      {
         ch_printf( ch, "%s is already open.\r\n", capitalize( obj->short_descr ) );
         return;
      }
      if( !IS_SET( obj->value[1], CONT_CLOSEABLE ) )
      {
         ch_printf( ch, "%s cannot be opened or closed.\r\n", capitalize( obj->short_descr ) );
         return;
      }
      if( IS_SET( obj->value[1], CONT_LOCKED ) )
      {
         ch_printf( ch, "%s is locked.\r\n", capitalize( obj->short_descr ) );
         return;
      }

      REMOVE_BIT( obj->value[1], CONT_CLOSED );
      act( AT_ACTION, "You open $p.", ch, obj, NULL, TO_CHAR );
      act( AT_ACTION, "$n opens $p.", ch, obj, NULL, TO_ROOM );
      check_for_trap( ch, obj, TRAP_OPEN );
      return;
   }

   if( !str_cmp( arg, "hatch" ) )
   {
      do_openhatch( ch, argument );
      return;
   }

   do_openhatch( ch, arg );
}

void do_close( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   EXIT_DATA *pexit;
   int door;

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      do_closehatch( ch, "" );
      return;
   }

   if( ( pexit = find_door( ch, arg, TRUE ) ) != NULL )
   {
      /*
       * 'close door' 
       */
      EXIT_DATA *pexit_rev;

      if( !IS_SET( pexit->exit_info, EX_ISDOOR ) )
      {
         send_to_char( "You can't do that.\r\n", ch );
         return;
      }
      if( IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "It's already closed.\r\n", ch );
         return;
      }

      act( AT_ACTION, "$n closes the $d.", ch, NULL, pexit->keyword, TO_ROOM );
      act( AT_ACTION, "You close the $d.", ch, NULL, pexit->keyword, TO_CHAR );

      /*
       * close the other side 
       */
      if( ( pexit_rev = pexit->rexit ) != NULL && pexit_rev->to_room == ch->in_room )
      {
         CHAR_DATA *rch;

         SET_BIT( pexit_rev->exit_info, EX_CLOSED );
         for( rch = pexit->to_room->first_person; rch; rch = rch->next_in_room )
            act( AT_ACTION, "The $d closes.", rch, NULL, pexit_rev->keyword, TO_CHAR );
      }
      set_bexit_flag( pexit, EX_CLOSED );
      if( ( door = pexit->vdir ) >= 0 && door < 10 )
         check_room_for_traps( ch, trap_door[door] );
      return;
   }

   if( ( obj = get_obj_here( ch, arg ) ) != NULL )
   {
      /*
       * 'close object' 
       */
      if( obj->item_type != ITEM_CONTAINER )
      {
         ch_printf( ch, "%s isn't a container.\r\n", capitalize( obj->short_descr ) );
         return;
      }
      if( IS_SET( obj->value[1], CONT_CLOSED ) )
      {
         ch_printf( ch, "%s is already closed.\r\n", capitalize( obj->short_descr ) );
         return;
      }
      if( !IS_SET( obj->value[1], CONT_CLOSEABLE ) )
      {
         ch_printf( ch, "%s cannot be opened or closed.\r\n", capitalize( obj->short_descr ) );
         return;
      }

      SET_BIT( obj->value[1], CONT_CLOSED );
      act( AT_ACTION, "You close $p.", ch, obj, NULL, TO_CHAR );
      act( AT_ACTION, "$n closes $p.", ch, obj, NULL, TO_ROOM );
      check_for_trap( ch, obj, TRAP_CLOSE );
      return;
   }

   if( !str_cmp( arg, "hatch" ) )
   {
      do_closehatch( ch, argument );
      return;
   }

   do_closehatch( ch, arg );
}

bool has_key( CHAR_DATA * ch, int key )
{
   OBJ_DATA *obj;

   for( obj = ch->first_carrying; obj; obj = obj->next_content )
      if( obj->pIndexData->vnum == key || obj->value[0] == key )
         return TRUE;

   return FALSE;
}

void do_lock( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   EXIT_DATA *pexit;

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "Lock what?\r\n", ch );
      return;
   }

   if( ( pexit = find_door( ch, arg, TRUE ) ) != NULL )
   {
      /*
       * 'lock door' 
       */
      if( !IS_SET( pexit->exit_info, EX_ISDOOR ) )
      {
         send_to_char( "You can't do that.\r\n", ch );
         return;
      }
      if( !IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "It's not closed.\r\n", ch );
         return;
      }
      if( pexit->key < 0 )
      {
         send_to_char( "It can't be locked.\r\n", ch );
         return;
      }
      if( !has_key( ch, pexit->key ) )
      {
         send_to_char( "You lack the key.\r\n", ch );
         return;
      }
      if( IS_SET( pexit->exit_info, EX_LOCKED ) )
      {
         send_to_char( "It's already locked.\r\n", ch );
         return;
      }

      if( !IS_SET( pexit->exit_info, EX_SECRET ) || ( pexit->keyword && nifty_is_name( arg, pexit->keyword ) ) )
      {
         send_to_char( "*Click*\r\n", ch );
         act( AT_ACTION, "$n locks the $d.", ch, NULL, pexit->keyword, TO_ROOM );
         set_bexit_flag( pexit, EX_LOCKED );
         return;
      }
   }

   if( ( obj = get_obj_here( ch, arg ) ) != NULL )
   {
      /*
       * 'lock object' 
       */
      if( obj->item_type != ITEM_CONTAINER )
      {
         send_to_char( "That's not a container.\r\n", ch );
         return;
      }
      if( !IS_SET( obj->value[1], CONT_CLOSED ) )
      {
         send_to_char( "It's not closed.\r\n", ch );
         return;
      }
      if( obj->value[2] < 0 )
      {
         send_to_char( "It can't be locked.\r\n", ch );
         return;
      }
      if( !has_key( ch, obj->value[2] ) )
      {
         send_to_char( "You lack the key.\r\n", ch );
         return;
      }
      if( IS_SET( obj->value[1], CONT_LOCKED ) )
      {
         send_to_char( "It's already locked.\r\n", ch );
         return;
      }

      SET_BIT( obj->value[1], CONT_LOCKED );
      send_to_char( "*Click*\r\n", ch );
      act( AT_ACTION, "$n locks $p.", ch, obj, NULL, TO_ROOM );
      return;
   }

   ch_printf( ch, "You see no %s here.\r\n", arg );
}

void do_unlock( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   EXIT_DATA *pexit;

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "Unlock what?\r\n", ch );
      return;
   }

   if( ( pexit = find_door( ch, arg, TRUE ) ) != NULL )
   {
      /*
       * 'unlock door' 
       */

      if( !IS_SET( pexit->exit_info, EX_ISDOOR ) )
      {
         send_to_char( "You can't do that.\r\n", ch );
         return;
      }
      if( !IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "It's not closed.\r\n", ch );
         return;
      }
      if( pexit->key < 0 )
      {
         send_to_char( "It can't be unlocked.\r\n", ch );
         return;
      }
      if( !has_key( ch, pexit->key ) )
      {
         send_to_char( "You lack the key.\r\n", ch );
         return;
      }
      if( !IS_SET( pexit->exit_info, EX_LOCKED ) )
      {
         send_to_char( "It's already unlocked.\r\n", ch );
         return;
      }

      if( !IS_SET( pexit->exit_info, EX_SECRET ) || ( pexit->keyword && nifty_is_name( arg, pexit->keyword ) ) )
      {
         send_to_char( "*Click*\r\n", ch );
         act( AT_ACTION, "$n unlocks the $d.", ch, NULL, pexit->keyword, TO_ROOM );
         remove_bexit_flag( pexit, EX_LOCKED );
         return;
      }
   }

   if( ( obj = get_obj_here( ch, arg ) ) != NULL )
   {
      /*
       * 'unlock object' 
       */
      if( obj->item_type != ITEM_CONTAINER )
      {
         send_to_char( "That's not a container.\r\n", ch );
         return;
      }
      if( !IS_SET( obj->value[1], CONT_CLOSED ) )
      {
         send_to_char( "It's not closed.\r\n", ch );
         return;
      }
      if( obj->value[2] < 0 )
      {
         send_to_char( "It can't be unlocked.\r\n", ch );
         return;
      }
      if( !has_key( ch, obj->value[2] ) )
      {
         send_to_char( "You lack the key.\r\n", ch );
         return;
      }
      if( !IS_SET( obj->value[1], CONT_LOCKED ) )
      {
         send_to_char( "It's already unlocked.\r\n", ch );
         return;
      }

      REMOVE_BIT( obj->value[1], CONT_LOCKED );
      send_to_char( "*Click*\r\n", ch );
      act( AT_ACTION, "$n unlocks $p.", ch, obj, NULL, TO_ROOM );
      return;
   }

   ch_printf( ch, "You see no %s here.\r\n", arg );
}

void do_bashdoor( CHAR_DATA * ch, const char *argument )
{
   EXIT_DATA *pexit;
   char arg[MAX_INPUT_LENGTH];

   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_bashdoor] <= 0 )
   {
      send_to_char( "You're not enough of a warrior to bash doors!\r\n", ch );
      return;
   }

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "Bash what?\r\n", ch );
      return;
   }

   if( ch->fighting )
   {
      send_to_char( "You can't break off your fight.\r\n", ch );
      return;
   }

   if( ( pexit = find_door( ch, arg, FALSE ) ) != NULL )
   {
      ROOM_INDEX_DATA *to_room;
      EXIT_DATA *pexit_rev;
      int schance;
      const char *keyword;

      if( !IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "Calm down.  It is already open.\r\n", ch );
         return;
      }

      WAIT_STATE( ch, skill_table[gsn_bashdoor]->beats );

      if( IS_SET( pexit->exit_info, EX_SECRET ) )
         keyword = "wall";
      else
         keyword = pexit->keyword;
      if( !IS_NPC( ch ) )
         schance = ch->pcdata->learned[gsn_bashdoor] / 2;
      else
         schance = 90;

      if( !IS_SET( pexit->exit_info, EX_BASHPROOF )
          && ch->move >= 15 && number_percent(  ) < ( schance + 4 * ( get_curr_str( ch ) - 19 ) ) )
      {
         REMOVE_BIT( pexit->exit_info, EX_CLOSED );
         if( IS_SET( pexit->exit_info, EX_LOCKED ) )
            REMOVE_BIT( pexit->exit_info, EX_LOCKED );
         SET_BIT( pexit->exit_info, EX_BASHED );

         act( AT_SKILL, "Crash!  You bashed open the $d!", ch, NULL, keyword, TO_CHAR );
         act( AT_SKILL, "$n bashes open the $d!", ch, NULL, keyword, TO_ROOM );
         learn_from_success( ch, gsn_bashdoor );

         if( ( to_room = pexit->to_room ) != NULL
             && ( pexit_rev = pexit->rexit ) != NULL && pexit_rev->to_room == ch->in_room )
         {
            CHAR_DATA *rch;

            REMOVE_BIT( pexit_rev->exit_info, EX_CLOSED );
            if( IS_SET( pexit_rev->exit_info, EX_LOCKED ) )
               REMOVE_BIT( pexit_rev->exit_info, EX_LOCKED );
            SET_BIT( pexit_rev->exit_info, EX_BASHED );

            for( rch = to_room->first_person; rch; rch = rch->next_in_room )
            {
               act( AT_SKILL, "The $d crashes open!", rch, NULL, pexit_rev->keyword, TO_CHAR );
            }
         }
         damage( ch, ch, ( ch->max_hit / 20 ), gsn_bashdoor );

      }
      else
      {
         act( AT_SKILL, "WHAAAAM!!!  You bash against the $d, but it doesn't budge.", ch, NULL, keyword, TO_CHAR );
         act( AT_SKILL, "WHAAAAM!!!  $n bashes against the $d, but it holds strong.", ch, NULL, keyword, TO_ROOM );
         damage( ch, ch, ( ch->max_hit / 20 ) + 10, gsn_bashdoor );
         learn_from_failure( ch, gsn_bashdoor );
      }
   }
   else
   {
      act( AT_SKILL, "WHAAAAM!!!  You bash against the wall, but it doesn't budge.", ch, NULL, NULL, TO_CHAR );
      act( AT_SKILL, "WHAAAAM!!!  $n bashes against the wall, but it holds strong.", ch, NULL, NULL, TO_ROOM );
      damage( ch, ch, ( ch->max_hit / 20 ) + 10, gsn_bashdoor );
      learn_from_failure( ch, gsn_bashdoor );
   }
}

void do_stand( CHAR_DATA * ch, const char *argument )
{
   switch ( ch->position )
   {
      case POS_SLEEPING:
         if( IS_AFFECTED( ch, AFF_SLEEP ) )
         {
            send_to_char( "You can't seem to wake up!\r\n", ch );
            return;
         }

         send_to_char( "You wake and climb quickly to your feet.\r\n", ch );
         act( AT_ACTION, "$n arises from $s slumber.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_STANDING;
         break;

      case POS_RESTING:
         send_to_char( "You gather yourself and stand up.\r\n", ch );
         act( AT_ACTION, "$n rises from $s rest.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_STANDING;
         break;

      case POS_SITTING:
         send_to_char( "You move quickly to your feet.\r\n", ch );
         act( AT_ACTION, "$n rises up.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_STANDING;
         break;

      case POS_STANDING:
         send_to_char( "You are already standing.\r\n", ch );
         break;

      case POS_FIGHTING:
         send_to_char( "You are already fighting!\r\n", ch );
         break;
   }
}

void do_sit( CHAR_DATA * ch, const char *argument )
{
   switch ( ch->position )
   {
      case POS_SLEEPING:
         if( IS_AFFECTED( ch, AFF_SLEEP ) )
         {
            send_to_char( "You can't seem to wake up!\r\n", ch );
            return;
         }

         send_to_char( "You wake and sit up.\r\n", ch );
         act( AT_ACTION, "$n wakes and sits up.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_SITTING;
         break;

      case POS_RESTING:
         send_to_char( "You stop resting and sit up.\r\n", ch );
         act( AT_ACTION, "$n stops resting and sits up.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_SITTING;
         break;

      case POS_STANDING:
         send_to_char( "You sit down.\r\n", ch );
         act( AT_ACTION, "$n sits down.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_SITTING;
         break;
      case POS_SITTING:
         send_to_char( "You are already sitting.\r\n", ch );
         return;

      case POS_FIGHTING:
         send_to_char( "You are busy fighting!\r\n", ch );
         return;
      case POS_MOUNTED:
         send_to_char( "You are already sitting - on your mount.\r\n", ch );
         return;
   }
}

void do_rest( CHAR_DATA * ch, const char *argument )
{
   switch ( ch->position )
   {
      case POS_SLEEPING:
         if( IS_AFFECTED( ch, AFF_SLEEP ) )
         {
            send_to_char( "You can't seem to wake up!\r\n", ch );
            return;
         }

         send_to_char( "You rouse from your slumber.\r\n", ch );
         act( AT_ACTION, "$n rouses from $s slumber.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_RESTING;
         break;

      case POS_RESTING:
         send_to_char( "You are already resting.\r\n", ch );
         return;

      case POS_STANDING:
         send_to_char( "You sprawl out haphazardly.\r\n", ch );
         act( AT_ACTION, "$n sprawls out haphazardly.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_RESTING;
         break;

      case POS_SITTING:
         send_to_char( "You lie back and sprawl out to rest.\r\n", ch );
         act( AT_ACTION, "$n lies back and sprawls out to rest.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_RESTING;
         break;

      case POS_FIGHTING:
         send_to_char( "You are busy fighting!\r\n", ch );
         return;
      case POS_MOUNTED:
         send_to_char( "You'd better dismount first.\r\n", ch );
         return;
   }

   rprog_rest_trigger( ch );
}

void do_sleep( CHAR_DATA * ch, const char *argument )
{
   switch ( ch->position )
   {
      case POS_SLEEPING:
         send_to_char( "You are already sleeping.\r\n", ch );
         return;

      case POS_RESTING:
         if( ch->mental_state > 30 && ( number_percent(  ) + 10 ) < ch->mental_state )
         {
            send_to_char( "You just can't seem to calm yourself down enough to sleep.\r\n", ch );
            act( AT_ACTION, "$n closes $s eyes for a few moments, but just can't seem to go to sleep.", ch, NULL, NULL,
                 TO_ROOM );
            return;
         }
         send_to_char( "You close your eyes and drift into slumber.\r\n", ch );
         act( AT_ACTION, "$n closes $s eyes and drifts into a deep slumber.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_SLEEPING;
         break;

      case POS_SITTING:
         if( ch->mental_state > 30 && ( number_percent(  ) + 5 ) < ch->mental_state )
         {
            send_to_char( "You just can't seem to calm yourself down enough to sleep.\r\n", ch );
            act( AT_ACTION, "$n closes $s eyes for a few moments, but just can't seem to go to sleep.", ch, NULL, NULL,
                 TO_ROOM );
            return;
         }
         send_to_char( "You slump over and fall dead asleep.\r\n", ch );
         act( AT_ACTION, "$n nods off and slowly slumps over, dead asleep.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_SLEEPING;
         break;

      case POS_STANDING:
         if( ch->mental_state > 30 && number_percent(  ) < ch->mental_state )
         {
            send_to_char( "You just can't seem to calm yourself down enough to sleep.\r\n", ch );
            act( AT_ACTION, "$n closes $s eyes for a few moments, but just can't seem to go to sleep.", ch, NULL, NULL,
                 TO_ROOM );
            return;
         }
         send_to_char( "You collapse into a deep sleep.\r\n", ch );
         act( AT_ACTION, "$n collapses into a deep sleep.", ch, NULL, NULL, TO_ROOM );
         ch->position = POS_SLEEPING;
         break;

      case POS_FIGHTING:
         send_to_char( "You are busy fighting!\r\n", ch );
         return;
      case POS_MOUNTED:
         send_to_char( "You really should dismount first.\r\n", ch );
         return;
   }

   rprog_sleep_trigger( ch );
}

void do_wake( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;

   one_argument( argument, arg );
   if( arg[0] == '\0' )
   {
      do_stand( ch, argument );
      return;
   }

   if( !IS_AWAKE( ch ) )
   {
      send_to_char( "You are asleep yourself!\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( IS_AWAKE( victim ) )
   {
      act( AT_PLAIN, "$N is already awake.", ch, NULL, victim, TO_CHAR );
      return;
   }

   if( IS_AFFECTED( victim, AFF_SLEEP ) || victim->position < POS_SLEEPING )
   {
      act( AT_PLAIN, "You can't seem to wake $M!", ch, NULL, victim, TO_CHAR );
      return;
   }

   act( AT_ACTION, "You wake $M.", ch, NULL, victim, TO_CHAR );
   victim->position = POS_STANDING;
   act( AT_ACTION, "$n wakes you.", ch, NULL, victim, TO_VICT );
}

/*
 * teleport a character to another room
 */
void teleportch( CHAR_DATA * ch, ROOM_INDEX_DATA * room, bool show )
{
   if( room_is_private( ch, room ) )
      return;
   act( AT_ACTION, "$n disappears suddenly!", ch, NULL, NULL, TO_ROOM );
   char_from_room( ch );
   char_to_room( ch, room );
   act( AT_ACTION, "$n arrives suddenly!", ch, NULL, NULL, TO_ROOM );
   if( show )
      do_look( ch, "auto" );
}

void teleport( CHAR_DATA * ch, int room, int flags )
{
   CHAR_DATA *nch, *nch_next;
   ROOM_INDEX_DATA *pRoomIndex;
   bool show;

   pRoomIndex = get_room_index( room );
   if( !pRoomIndex )
   {
      bug( "%s: bad room vnum %d", __func__, room );
      return;
   }

   if( IS_SET( flags, TELE_SHOWDESC ) )
      show = TRUE;
   else
      show = FALSE;
   if( !IS_SET( flags, TELE_TRANSALL ) )
   {
      teleportch( ch, pRoomIndex, show );
      return;
   }
   for( nch = ch->in_room->first_person; nch; nch = nch_next )
   {
      nch_next = nch->next_in_room;
      teleportch( nch, pRoomIndex, show );
   }
}

/*
 * "Climb" in a certain direction.				-Thoric
 */
void do_climb( CHAR_DATA * ch, const char *argument )
{
   EXIT_DATA *pexit;

   if( argument[0] == '\0' )
   {
      for( pexit = ch->in_room->first_exit; pexit; pexit = pexit->next )
         if( IS_SET( pexit->exit_info, EX_xCLIMB ) || IS_SET( pexit->exit_info, EX_CLIMB ) )
         {
            move_char( ch, pexit, 0 );
            return;
         }
      send_to_char( "You cannot climb here.\r\n", ch );
      return;
   }

   if( ( pexit = find_door( ch, argument, TRUE ) ) != NULL
    && ( IS_SET( pexit->exit_info, EX_xCLIMB ) || IS_SET( pexit->exit_info, EX_CLIMB ) ) )
   {
      move_char( ch, pexit, 0 );
      return;
   }
   send_to_char( "You cannot climb there.\r\n", ch );
}

/*
 * "enter" something (moves through an exit)			-Thoric
 */
void do_enter( CHAR_DATA * ch, const char *argument )
{
   EXIT_DATA *pexit;

   if( argument[0] == '\0' )
   {
      for( pexit = ch->in_room->first_exit; pexit; pexit = pexit->next )
         if( IS_SET( pexit->exit_info, EX_xENTER ) )
         {
            move_char( ch, pexit, 0 );
            return;
         }
      send_to_char( "You cannot find an entrance here.\r\n", ch );
      return;
   }

   if( ( pexit = find_door( ch, argument, TRUE ) ) != NULL && IS_SET( pexit->exit_info, EX_xENTER ) )
   {
      move_char( ch, pexit, 0 );
      return;
   }
   do_board( ch, argument );
}

/*
 * Leave through an exit.					-Thoric
 */
void do_leave( CHAR_DATA * ch, const char *argument )
{
   EXIT_DATA *pexit;

   if( argument[0] == '\0' )
   {
      for( pexit = ch->in_room->first_exit; pexit; pexit = pexit->next )
         if( IS_SET( pexit->exit_info, EX_xLEAVE ) )
         {
            move_char( ch, pexit, 0 );
            return;
         }
      do_leaveship( ch, "" );
      return;
   }

   if( ( pexit = find_door( ch, argument, TRUE ) ) != NULL && IS_SET( pexit->exit_info, EX_xLEAVE ) )
   {
      move_char( ch, pexit, 0 );
      return;
   }
   do_leaveship( ch, "" );
}
