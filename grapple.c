#include "g_local.h"
#include "grapple.h"


#define HOOK_TIME        5000   
#define HOOK_SPEED        1200   
#define THINK_TIME        0.3        
#define HOOK_DAMAGE        5        
#define GRAPPLE_REFIRE    2    
#define PULL_SPEED    500 


void P_ProjectSource (gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);

qboolean Ended_Grappling (gclient_t *client)
{
    return (!(client->buttons & BUTTON_USE) && client->oldbuttons & BUTTON_USE);
}

qboolean Is_Grappling (gclient_t *client)
{
    return (client->hook == NULL) ? false : true;
}

void Grapple_Touch(edict_t *hook, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    // Release if hitting its owner
    if (other == hook->owner)
        return;
    if (!Is_Grappling(hook->owner->client) && hook->health == 0) {
        return;
    }

    hook->health = 0;
    if (surf && surf->flags & SURF_SKY)
        {
            Release_Grapple(hook);
            return;
    }

    if (other != g_edicts && other->clipmask == MASK_SHOT)
        return;
    gi.WriteByte (svc_temp_entity);
    gi.WriteByte (TE_BLASTER);
    gi.WritePosition (hook->s.origin);
    gi.WriteDir (plane->normal);
    gi.multicast (hook->s.origin, MULTICAST_PVS);
    //gi.sound(hook, CHAN_ITEM, gi.soundindex("hook/hit.wav"), 1, ATTN_NORM, 0);//uncomment this line to make it play a sound         //when your hook hits a wall

   if (other != NULL) {
        T_Damage(other, hook, hook->owner, hook->velocity, hook->s.origin, plane->normal, HOOK_DAMAGE, 0, 0, MOD_SUICIDE);
    }
    if (other != g_edicts && other->health && other->solid == SOLID_BBOX) {
        Release_Grapple(hook);
        return;
    }

    if (other != g_edicts && other->inuse &&
        (other->movetype == MOVETYPE_PUSH || other->movetype == MOVETYPE_STOP))
    {
        other->mynoise2 = hook;
        hook->owner->client->hook_touch = other;
        hook->enemy = other;
        hook->groundentity = NULL;
        hook->flags |= FL_TEAMSLAVE;
    }

    VectorClear(hook->velocity);
    VectorClear(hook->avelocity);
    hook->solid = SOLID_NOT;
    hook->touch = NULL;
    hook->movetype = MOVETYPE_NONE;
    hook->delay = level.time + HOOK_TIME;
    hook->owner->client->on_hook = true;
    hook->owner->groundentity = NULL;
    Pull_Grapple(hook->owner);

}

void Think_Grapple(edict_t *hook)
{
    if (level.time > hook->delay)
        hook->prethink = Release_Grapple;
    else
    {
        if (hook->owner->client->hook_touch) {
            edict_t *obj = hook->owner->client->hook_touch;

            if (obj == g_edicts)
            {
                Release_Grapple(hook);
                return;
            }

            if (obj->inuse == false) {
                Release_Grapple(hook);
                return;
            }

            if (obj->deadflag == DEAD_DEAD)
            {
                Release_Grapple(hook);
                return;
            }

            // Movement code is handled with the MOVETYPE_PUSH stuff in g_phys.c

            T_Damage(obj, hook, hook->owner, hook->velocity, hook->s.origin, vec3_origin, HOOK_DAMAGE, 0, 0, MOD_SUICIDE);
        }

        hook->nextthink += THINK_TIME;
    }
}

static void DrawBeam (edict_t *ent)
{
    gi.WriteByte (svc_temp_entity);
    gi.WriteByte (TE_BFG_LASER);
    gi.WritePosition (ent->owner->s.origin);
    gi.WritePosition (ent->s.origin);
    gi.multicast (ent->s.origin, MULTICAST_PHS);
}
void Make_Hook(edict_t *ent)
{
    edict_t *hook;
    vec3_t forward, right, start, offset;

    hook = G_Spawn();
    AngleVectors(ent->client->v_angle, forward, right, NULL);
    VectorScale(forward, -2, ent->client->kick_origin);
    ent->client->kick_angles[0] = -1;
    VectorSet(offset, 8, 0, ent->viewheight-8);
    P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

    VectorCopy(start, hook->s.origin);
    VectorCopy(forward, hook->movedir);
    vectoangles(forward, hook->s.angles);
    VectorScale(forward, HOOK_SPEED, hook->velocity);
    VectorSet(hook->avelocity, 0, 0, 500);

    hook->classname = "hook";
    hook->movetype = MOVETYPE_FLYMISSILE;
    hook->clipmask = MASK_SHOT;
    hook->solid = SOLID_BBOX;
    hook->svflags |= SVF_DEADMONSTER;
    hook->s.renderfx = RF_FULLBRIGHT;
    VectorClear (hook->mins);
    VectorClear (hook->maxs);
    hook->s.effects |= EF_COLOR_SHELL;
    hook->s.renderfx |= RF_SHELL_GREEN;   
    hook->s.modelindex = gi.modelindex ("models/objects/flash/tris.md2");   
    hook->owner = ent;
    hook->touch = Grapple_Touch;
    hook->delay = level.time + HOOK_TIME;
    hook->nextthink = level.time;

    hook->prethink = DrawBeam;
    hook->think = Think_Grapple;
    hook->health = 100;
    hook->svflags = SVF_MONSTER;
   
    ent->client->hook = hook;
    gi.linkentity(hook);
}

void Throw_Grapple (edict_t *player)
{
   
    if (player->client->hook) {
        return;
    }

    gi.sound(player, CHAN_ITEM, gi.soundindex("medic/medatck2.wav"), 0.5, ATTN_NORM, 0);

    player->client->hook_touch = NULL;
   
    Make_Hook(player);
}

void Release_Grapple (edict_t *hook)
{
    edict_t *owner = hook->owner;
    gclient_t *client = hook->owner->client;
    edict_t *link = hook->teamchain;

    client->on_hook = false;
    client->hook_touch = NULL;

    if (client->hook != NULL) {
        client->hook = NULL;
        VectorClear(client->oldvelocity);

        hook->think = NULL;

        if (hook->enemy) {
            hook->enemy->mynoise2 = NULL;
        }

        G_FreeEdict(hook);
    }
}

void Pull_Grapple (edict_t *player)
{
    vec3_t hookDir;
    vec_t length;

    VectorSubtract(player->client->hook->s.origin, player->s.origin, hookDir);
    length = VectorNormalize(hookDir);

    VectorScale(hookDir, /*player->scale * */ PULL_SPEED, player->velocity);
    VectorCopy(hookDir, player->movedir);

//To move the player off the ground just a bit so he doesn't stay stuck (version 3.17 bug)
    if (player->velocity[2] > 0) {

        vec3_t traceTo;
        trace_t trace;

        // find the point immediately above the player's origin
        VectorCopy(player->s.origin, traceTo);
        traceTo[2] += 1;

        // trace to it
        trace = gi.trace(traceTo, player->mins, player->maxs, traceTo, player, MASK_PLAYERSOLID);

        // if there isn't a solid immediately above the player
        if (!trace.startsolid) {
            player->s.origin[2] += 1;    // make sure player off ground
        }
    }

}