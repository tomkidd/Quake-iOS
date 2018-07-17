//
//  in_ios.c
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/13/16.
//
//

#include "quakedef.h"

float   in_forwardmove;
float   in_sidestepmove;

float   in_pitchangle;
float   in_rollangle;

cvar_t	in_anglescaling = {"in_anglescaling","0.1"};

void IN_Init (void)
{
    Cvar_RegisterVariable (&in_anglescaling);
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

void IN_Move (usercmd_t *cmd)
{
    cl.viewangles[YAW] -= in_rollangle * in_anglescaling.value * 90;
    
    V_StopPitchDrift();
    
    if (tiltaim_enabled) {
        cl.viewangles[PITCH] = (in_pitchangle + 1.5) * 90;
    } else {
        cl.viewangles[PITCH] += in_pitchangle * in_anglescaling.value * 90;
    }
    
    if (cl.viewangles[PITCH] > 80)
        cl.viewangles[PITCH] = 80;
    if (cl.viewangles[PITCH] < -70)
        cl.viewangles[PITCH] = -70;

    if (key_dest == key_game)
    {
        float speed;
        
        if (in_speed.state & 1)
            speed = cl_movespeedkey.value;
        else
            speed = 1;
        
        cmd->sidemove += in_forwardmove * speed * cl_forwardspeed.value;
        
        cmd->forwardmove += in_sidestepmove * speed * cl_sidespeed.value;
    }
}
