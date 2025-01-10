#ifndef PSRC_COMMON_ENTITIES_H
#define PSRC_COMMON_ENTITIES_H

#include "../attrib.h"

PACKEDENUM ent {
    ENT_DUMMY,
    ENT_HEADLESS,
    ENT_GENERIC,
    ENT_PROP,
    ENT_CUBE,
    ENT_DOOR,
    ENT_TRIGGER,
    ENT_BUTTON,
    ENT_SWITCH,
    ENT_VALVE,
    ENT_MOUNTABLE,
    ENT_HURTABLE,
    ENT_BEAM,
    ENT_PARTICLES,
    ENT_GIBS,
    ENT_GLOW,
    ENT_TEXT,
    ENT_BILLBLOARD,
    ENT_DECAL,
    ENT_PROJECTILES,
    ENT_PROJECTILE,
    ENT_HEALTH,
    ENT_PUSH,
    ENT_BROADCASTER,
    ENT_DISPATCHER,
    ENT_COUNTER,
    ENT_EXCOUNTER,
    ENT_TIMER,
    ENT_EXTIMER,
    ENT_SETTER,
    ENT_AUDIOEMITTER,
    ENT_DYNLIGHTCTL,
    ENT_FASTLIGHTCTL,
    ENT_SOUNDENV,
    ENT_WEATHERENV,
    ENT_GRAVITYENV,
    ENT__COUNT,
    ENT_PLAYER = 255
};

#define VALIDENT(t) ((t) < ENT__COUNT || (t) == ENT_PLAYER)

#endif
