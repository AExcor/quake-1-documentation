/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#if defined(USE_SDL2)
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif
#else
#include "SDL.h"
#endif
#include <stdio.h>

#if defined(USE_SDL2)

/* need at least SDL_2.0.0 */
#define SDL_MIN_X	2
#define SDL_MIN_Y	0
#define SDL_MIN_Z	0
#define SDL_REQUIREDVERSION	(SDL_VERSIONNUM(SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z))
#define SDL_NEW_VERSION_REJECT	(SDL_VERSIONNUM(3,0,0))

#else

/* need at least SDL_1.2.10 */
#define SDL_MIN_X	1
#define SDL_MIN_Y	2
#define SDL_MIN_Z	10
#define SDL_REQUIREDVERSION	(SDL_VERSIONNUM(SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z))
/* reject 1.3.0 and newer at runtime. */
#define SDL_NEW_VERSION_REJECT	(SDL_VERSIONNUM(1,3,0))

#endif

static void Sys_AtExit (void)
{
	SDL_Quit();
}

static void Sys_InitSDL (void)
{
#if defined(USE_SDL2)
	SDL_version v;
	SDL_version *sdl_version = &v;
	SDL_GetVersion(&v);
#else
	const SDL_version *sdl_version = SDL_Linked_Version();
#endif

	Sys_Printf("Found SDL version %i.%i.%i\n",sdl_version->major,sdl_version->minor,sdl_version->patch);
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) < SDL_REQUIREDVERSION)
	{	/*reject running under older SDL versions */
		Sys_Error("You need at least v%d.%d.%d of SDL to run this game.", SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z);
	}
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) >= SDL_NEW_VERSION_REJECT)
	{	/*reject running under newer (1.3.x) SDL */
		Sys_Error("Your version of SDL library is incompatible with me.\n"
			  "You need a library version in the line of %d.%d.%d\n", SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z);
	}

	if (SDL_Init(0) < 0)
	{
		Sys_Error("Couldn't init SDL: %s", SDL_GetError());
	}
	atexit(Sys_AtExit);
}

#define DEFAULT_MEMORY (256 * 1024 * 1024) // ericw -- was 72MB (64-bit) / 64MB (32-bit)

static quakeparms_t	parms;

// On OS X we call SDL_main from the launcher, but SDL2 doesn't redefine main
// as SDL_main on OS X anymore, so we do it ourselves.
#if defined(USE_SDL2) && defined(__APPLE__)
#define main SDL_main
#endif

//includes host.c
#include "quakedef.h"
#include "bgmusic.h"
#include <setjmp.h>
void _Host_Frame (float time);
extern cvar_t	serverprofile;
//includes-end host.c
int main(int argc, char *argv[])
{
	double		time, oldtime, newtime;

	host_parms = &parms;
	parms.basedir = ".";

	parms.argc = argc;
	parms.argv = argv;

	parms.errstate = 0;

	COM_InitArgv(parms.argc, parms.argv);

	Sys_InitSDL ();

	Sys_Init();

	parms.memsize = DEFAULT_MEMORY;
	parms.membase = malloc (parms.memsize);

	Sys_Printf("Quake %1.2f (c) id Software\nGLQuake %1.2f (c) id Software\nFitzQuake %1.2f (c) John Fitzgibbons\nFitzQuake SDL port (c) SleepwalkR, Baker\nQuakeSpasm " QUAKESPASM_VER_STRING " (c) Ozkan Sezer, Eric Wasylishen & others\n", VERSION, GLQUAKE_VERSION, FITZQUAKE_VERSION);

	//Sys_Printf("Host_Init\n");
	Host_Init();

	//not needed	
	oldtime = Sys_DoubleTime();

	//added by quake-1-documentation
	//allow a map to start immediately

	Cmd_ExecuteString("map start\n", src_command);

	if (isDedicated){
		printf("Removed Dedicated\n");
	}
	else
	while (1){
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		//Host_Frame (time);
		//startfunc Host_Frame
		//void Host_Frame (float time)
		{
			double	time1, time2;
			static double	timetotal;
			static int		timecount;
			int		i, c, m;

			if (!serverprofile.value)
			{
				_Host_Frame (time);
				goto skip_Host_Frame;				
				//return;
			}

			time1 = Sys_DoubleTime ();
			//_Host_Frame (time);
			//startfunc _Host_Frame
			//void _Host_Frame (float time)
			{
				static double		time1 = 0;
				static double		time2 = 0;
				static double		time3 = 0;
				int			pass1, pass2, pass3;

				if (setjmp (host_abortserver) )
					return;			// something bad happened, or the server disconnected

			// keep the random time dependent
				rand ();

			// decide the simulation time
				if (!Host_FilterTime (time))
					return;			// don't run too fast, or packets will flood out

			// get new key events
				Key_UpdateForDest ();
				IN_UpdateInputMode ();
				Sys_SendKeyEvents ();

			// allow mice or other external controllers to add commands
				IN_Commands ();

			// process console commands
				Cbuf_Execute ();

				NET_Poll();

			// if running the server locally, make intentions now
				if (sv.active)
					CL_SendCmd ();

			//-------------------
			//
			// server operations
			//
			//-------------------

			// check for commands typed to the host
				Host_GetConsoleCommands ();

				if (sv.active)
					Host_ServerFrame ();

			//-------------------
			//
			// client operations
			//
			//-------------------

			// if running the server remotely, send intentions now after
			// the incoming messages have been read
				if (!sv.active)
					CL_SendCmd ();

			// fetch results from server
				if (cls.state == ca_connected)
					CL_ReadFromServer ();

			// update video
				if (host_speeds.value)
					time1 = Sys_DoubleTime ();

				SCR_UpdateScreen ();

				CL_RunParticles (); //johnfitz -- seperated from rendering

				if (host_speeds.value)
					time2 = Sys_DoubleTime ();

			// update audio
				BGM_Update();	// adds music raw samples and/or advances midi driver
				if (cls.signon == SIGNONS)
				{
					S_Update (r_origin, vpn, vright, vup);
					CL_DecayLights ();
				}
				else
					S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

				CDAudio_Update();

				if (host_speeds.value)
				{
					pass1 = (time1 - time3)*1000;
					time3 = Sys_DoubleTime ();
					pass2 = (time2 - time1)*1000;
					pass3 = (time3 - time2)*1000;
					Con_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
								pass1+pass2+pass3, pass1, pass2, pass3);
				}

				host_framecount++;

			}
			//endfunc _Host_Frame
			time2 = Sys_DoubleTime ();

			timetotal += time2 - time1;
			timecount++;

			if (timecount < 1000){
				goto skip_Host_Frame;
				//return;
			}

			m = timetotal*1000/timecount;
			timecount = 0;
			timetotal = 0;
			c = 0;
			for (i = 0; i < svs.maxclients; i++)
			{
				if (svs.clients[i].active)
					c++;
			}

			Con_Printf ("serverprofile: %2i clients %2i msec\n",  c,  m);
			skip_Host_Frame:;		
		}
		//endfunc Host_Frame
		oldtime = newtime;
	}
	return 0;
}

