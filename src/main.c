#include "modding.h"
#include "ultra64.h"
#include "enums.h"
#include "common_structs.h"
#include "recompconfig.h"

typedef struct {
    u8  loaded;
    u8  unk1; // used
    u8  unk2;
    u8  unk3;
    u8  unk4;
    u8  visible; // 0x5 -- if 0x02 visible, else invisible
    u8  unk6;
    u8  unk7;
    s32 unk8;
    s32 unkC;
    s32 unk10;
    void *unk14;
    s32 unk18;
    void *unk1C; // TODO: Same struct as unk14?
    s32 unk20;
    s32 unk24;
    u8  pad8[0x4C - 0x28];
    void *unk4C;
    s32 unk50;
    u8  pad54[0x60 - 0x54];
    s32 unk60[1]; // TODO: How many?
    s32 unk64;
    s32 deload1; // 0x68
    s32 deload2; // 0x6C
    s32 deload3; // 0x70
    s32 deload4; // 0x74
    void *unk78; // First in array?
    void *unk7C; // Last in array?
    s16 unk80; // Used
    s16 unk82; // Used
    s16 unk84; // Used
    s16 unk86; // Used
    u8  pad2[0x1C8 - 0x88]; // total size 0x1C8
} Chunk;

f32 cycle_time = 0.0f;
u32 last_igt = 0;
u8 stage = 0; // 0 = becoming day, 1 = becoming night
u8 sub_ticker = 0;

u32 func_global_asm_805FC98C(void); // get igt
f32 func_global_asm_80612790(s16 arg0); // cos function
f32 func_global_asm_80612794(s16 arg0);
void func_global_asm_80659670(f32 arg0, f32 arg1, f32 arg2, s16 arg3); // set chunk lighting
extern s32 D_global_asm_807F6C28; // Chunk count
extern u32 global_properties_bitfield;
extern Chunk *chunk_array_pointer;
extern Maps current_map;
extern f32 *D_global_asm_8076A0C0; // pointer to an array of floats
extern f32 *D_global_asm_8076A0C4;
extern f32 *D_global_asm_8076A0C8;

s32 getCycleLength(void) {
    return 60 * recomp_get_config_u32("cycle_length");
}

f32 getDayNightPhase(u8 *stg) {
    u32 igt;
    s32 phase;
    s32 cycle_length;
    f32 seconds;

    // 0 = Night
    // 1 = Day
    igt = func_global_asm_805FC98C();
    if (last_igt != igt) {
        sub_ticker = 0;
    }
    last_igt = igt;
    cycle_length = getCycleLength();
    seconds = igt % cycle_length;
    seconds += (sub_ticker / 30.0f);
    phase = (seconds / (f32)(cycle_length)) * 4096;
    phase &= 0xFFF;
    if (phase < 2048) {
        *stg = 1;
    } else {
        *stg = 0;
    }
    return (func_global_asm_80612790(phase) + 1.0f) / 2.0f;
}

void getTimeRGB(f32 *r, f32 *g, f32 *b, f32 time) {
    f32 red_darkness;

    red_darkness = recomp_get_config_u32("night_darkness") / 100.0f;
    *r = red_darkness + ((1.0f - red_darkness) * time);
    *g = *r;
    *b = (2 * red_darkness) + ((1.0f - (2 * red_darkness)) * time);
}

#define SUNSET_RANGE 0.05f
#define SUNSET_FOCAL 0.4f
void getDayNightLighting(f32 *r, f32 *g, f32 *b) {
    f32 target_r;
    f32 target_g;
    f32 target_b;
    f32 reference_r;
    f32 reference_g;
    f32 reference_b;
    f32 delta;
    f32 delta_r;
    f32 delta_g;
    f32 delta_b;
    f32 target_time;

    getTimeRGB(r, g, b, cycle_time);
    if (recomp_get_config_u32("sunset_enabled")) {
        if ((cycle_time >= (SUNSET_FOCAL - SUNSET_RANGE)) && (cycle_time <= (SUNSET_FOCAL + SUNSET_RANGE))) {
            target_time = SUNSET_FOCAL - SUNSET_RANGE;
            if (cycle_time > SUNSET_FOCAL) {
                target_time = SUNSET_FOCAL + SUNSET_RANGE;
            }
            getTimeRGB(&reference_r, &reference_g, &reference_b, target_time);
            target_r = 1.0f;
            target_g = 0.67f;
            target_b = 0.75f;
            delta_r = target_r - reference_r;
            delta_g = target_g - reference_g;
            delta_b = target_b - reference_b;
            delta = (SUNSET_FOCAL - cycle_time) * (1.0f / SUNSET_RANGE);
            if (delta < 0.0f) {
                delta = -delta;
            }
            delta = 1.0f - delta;
            *r = reference_r + (delta * delta_r);
            *g = reference_g + (delta * delta_g);
            *b = reference_b + (delta * delta_b);
        }
    }
}

u8 allowed_maps[] = {
    MAP_JAPES,
    MAP_JAPES_ARMY_DILLO,
    MAP_GALLEON,
    MAP_DK_ISLES_OVERWORLD,
    MAP_JAPES_BARREL_BLAST,
    MAP_AZTEC,
    MAP_GALLEON_SEAL_RACE,
    MAP_AZTEC_BARREL_BLAST,
    MAP_FUNGI,
    MAP_GALLEON_BARREL_BLAST,
    MAP_FUNGI_MINECART,
    MAP_CASTLE,
    MAP_GALLEON_PUFFTOSS,
    MAP_DK_ISLES_DK_THEATRE,
    MAP_DK_HOUSE,
    MAP_ROCK_INTRO_STORY,
    MAP_TRAINING_GROUNDS,
    MAP_CASTLE_BARREL_BLAST,
    MAP_FUNGI_BARREL_BLAST,
    MAP_TRAINING_GROUNDS_END_SEQUENCE,
    MAP_CASTLE_KING_KUT_OUT,
    MAP_KLUMSY_ENDING,
};

typedef struct {
    Actor* unk0;
    s32 unk4;
} GlobalASMStruct53;

extern u16 D_global_asm_807FBB34;
extern GlobalASMStruct53 D_global_asm_807FB930[];

u8 impactedByDayNight(void) {
    u32 i;

    if ((global_properties_bitfield & 0x10) == 0) {
        return FALSE;
    }
    
    for (i = 0; i < sizeof(allowed_maps); i++) {
        if (current_map == allowed_maps[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

void setDayNightLighting(void) {
    f32 r, g, b;
    s32 j;
    u16 k;
    f32 *lzcontrolleraad;

    if (!impactedByDayNight()) {
        return;
    }
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
    getDayNightLighting(&r, &g, &b);
    for (j = 0; j < D_global_asm_807F6C28; j++) {
        func_global_asm_80659670(r, g, b, j);
        chunk_array_pointer[j].unk3 = 1;
    }
    for (k = 0; k < D_global_asm_807FBB34; k++) {
        if (D_global_asm_807FB930[k].unk0) {
            if (D_global_asm_807FB930[k].unk0->unk58 == ACTOR_LOADING_ZONE_CONTROLLER) {
                lzcontrolleraad = D_global_asm_807FB930[k].unk0->AAD_as_array[0];
                lzcontrolleraad[3] = r;
                lzcontrolleraad[4] = g;
                lzcontrolleraad[5] = b;
            }
        }
    }
}

void func_global_asm_80659620(f32 *arg0, f32 *arg1, f32 *arg2, s16 arg3);
void func_global_asm_8070033C(f32, f32, f32, f32, f32, f32, f32, u32, u32, u32);
void drawSun(s16 arg0, s32 arg1, u8 arg2) {
    f32 sp4C;
    f32 sp48;
    f32 sp44;
    s16 sp42;
    s16 temp_f4; // sp40
    s16 temp_v1; // sp3E

    sp42 = func_global_asm_80612794(arg0) * 32767.0;
    temp_v1 = func_global_asm_80612790(arg0) * 32767.0;
    temp_f4 = arg1 - (8.0 * character_change_array->unk2CA);
    func_global_asm_8070033C(sp42, temp_f4, temp_v1, 0.0f, 0.0f, 0.0f, 0.0f, 0xFF, 0xFF, 0xFF);
}

// extern f32 D_global_asm_80754CE8;
// extern Mtx D_2000100;
// extern Actor *gPlayerPointer;
// s32 func_global_asm_80626F8C(f32, f32, f32, f32 *, f32 *, s32, f32, s32);
// Gfx *func_global_asm_806FEDB0(Gfx *dl, u8 arg1);
// Gfx *displayImage(Gfx *dl, u16 textureIndex, s32 arg3, s32 codec, s32 width, s32 height, s32 x, s32 y, f32 xScale, f32 yScale, s32 arg11, f32 arg12);

// Gfx *drawMoon(Gfx *dl, s32 arg1, s32 arg2, u8 arg3) {
//     f32 sp76;
//     f32 sp74;
//     f32 sp72;
//     f32 x;
//     f32 y;
//     u8 alpha;
//     f32 sp58[3];
    
//     gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
//     func_global_asm_80626F8C(
//         func_global_asm_80612794(arg1) * 32767.0,
//         arg2 - (8.0 * character_change_array->unk2CA),
//         func_global_asm_80612790(arg1) * 32767.0,
//         &x, &y, 0, 4.0f, 0);
//     if ((x > -320.0f) && (x < 1600.0f) && (y > -240.0f) && (y < 1200.0f)) {
//         dl = func_global_asm_806FEDB0(dl, 0);
//         gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
//         gSPMatrix(dl++, &D_2000100, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
//         dl = displayImage(dl, 0x35, 3, 1, 0x40, 0x40, x, y, 1.0f, 1.0f, 0, 0.0f);
//     }
//     return dl;
// }

RECOMP_CALLBACK("*", dk64recomp_every_frame) void setDayNight(void) {
    cycle_time = getDayNightPhase(&stage); // Ensure this is done max 1x per frame
    // cycle_time = 0.0f;
    setDayNightLighting();
    sub_ticker++;
}

extern Actor *gPlayerPointer;
Gfx* func_global_asm_80704B20(Gfx* dl, f32 arg1, f32 arg2, Mtx* arg3, u8 arg4, u8 arg5, u8 arg6, s8 arg7, f32 arg8);
Gfx* func_global_asm_80707980(Gfx* dl, f32 arg1, f32 arg2, Mtx *arg3, s16 arg4);
Gfx* func_global_asm_807069A4(Gfx* dl, f32 arg1, f32 arg2, s32 arg3, f32 arg4, f32 arg5);
Gfx* func_global_asm_80705F5C(Gfx*, s16, s16, s16);
Gfx *func_global_asm_8070770C(Gfx *);
void func_global_asm_80705C00(s16 arg0, s16 arg1, u8 arg2);
void func_global_asm_8068B830(s16 arg0, s16 arg1, s16 arg2);
void func_global_asm_8068B8A4(f32 arg0);
void func_global_asm_8068B8FC(void);
u8 func_global_asm_8061CB50(void);
f32 sqrtf(f32);
extern f32 D_global_asm_80754CE8;
extern s16 D_global_asm_807FD800;
extern f32 loading_zone_transition_speed;
extern u8 loading_zone_transition_type;

#define PEAK_HEIGHT 96000

RECOMP_FORCE_PATCH Gfx* func_global_asm_80707980(Gfx* dl, f32 arg1, f32 arg2, Mtx *arg3, s16 arg4) {
    f32 temp_f0;
    f32 var_f2;
    f32 temp_f2;
    f32 var_f14;
    f32 var_f16;
    f64 temp_f12;
    // s16 temp_v0;
    s32 var_v0;

    gDPPipeSync(dl++);
    if ((loading_zone_transition_speed != 0.0f) && (loading_zone_transition_type == 3)) {
        dl = func_global_asm_8070770C(dl);
    }
    if (impactedByDayNight()) {
        if (cycle_time > (0.5f - SUNSET_RANGE)) {
            // Partially day
            drawSun(stage == 0 ? 3000 : 952, PEAK_HEIGHT * ((cycle_time - (0.5f - SUNSET_RANGE)) * (1.0f / (0.5f + SUNSET_RANGE))), 0U);
        }
    }
    switch (current_map) {
        case MAP_FUNGI_DOGADON:
        case MAP_FACTORY_MAD_JACK:
        case MAP_AZTEC_DOGADON:
        case MAP_TRAINING_GROUNDS_END_SEQUENCE:
            return dl;
        case MAP_AZTEC_BEETLE_RACE:
            dl = func_global_asm_807069A4(dl, arg1, arg2, 0x2D, 320.0f, 240.0f);
            break;
        case MAP_KROOL_BARREL_LANKY_MAZE:
        case MAP_STEALTHY_SNOOP_NORMAL_NO_LOGO:
        case MAP_STEALTHY_SNOOP_NORMAL:
        case MAP_MAD_MAZE_MAUL_HARD:
        case MAP_STASH_SNATCH_NORMAL:
        case MAP_MAD_MAZE_MAUL_EASY:
        case MAP_MAD_MAZE_MAUL_NORMAL:
        case MAP_STASH_SNATCH_EASY:
        case MAP_STASH_SNATCH_HARD:
        case MAP_MAD_MAZE_MAUL_INSANE:
        case MAP_STASH_SNATCH_INSANE:
        case MAP_STEALTHY_SNOOP_VERY_EASY:
        case MAP_STEALTHY_SNOOP_EASY:
        case MAP_STEALTHY_SNOOP_HARD:
            dl = func_global_asm_807069A4(dl, arg1, arg2, 0x2E, 320.0f, 240.0f);
            break;
        case MAP_AZTEC:
            switch (character_change_array->chunk) {
                case 0:
                case 1:
                case 3:
                case 6:
                case 8:
                case 10:
                    dl = func_global_asm_8070770C(dl);
                    break;
                default:
                    // func_global_asm_80705C00(0x3E8, 0x4E20, 0U);
                    dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 0, 0, 0, -1, 0.0f);
                    break;
            }
            break;
        case MAP_GALLEON:
            // temp_v0 = character_change_array->chunk;
            // @recomp: Change chunk checks
            // if (((temp_v0 != 7) && (temp_v0 != 6) && (temp_v0 != 8) && (temp_v0 != 0)) || (D_global_asm_807FD800 != 0)) {
                if ((gPlayerPointer->unk12C == 9) || (gPlayerPointer->unk12C == 0xB) || (gPlayerPointer->unk12C == 3)) {
                    var_v0 = 1;
                } else {
                    var_v0 = 0;
                }
                dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 0, 0, var_v0, 9, 0.0f);
            // }
            break;
        case MAP_GALLEON_SEAL_RACE:
            // func_global_asm_80705C00(0xBB8, 0x2328, 0U);
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 0, 0, 0, -1, 0.0f);
            break;
        case MAP_JAPES_MOUNTAIN:
            dl = func_global_asm_8070770C(dl);
            break;
        case MAP_TRAINING_GROUNDS:
            dl = func_global_asm_8070770C(dl);
            break;
        case MAP_JAPES:
            switch (character_change_array->chunk) {
            case 8:
            case 9:
            case 12:
            case 16:
                dl = func_global_asm_8070770C(dl);
                break;
            case 11:
            case 14:
                // func_global_asm_80705C00(0, 0x7530, 1U);
                dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 4, 4, 1, -1, 0.0f);
                break;
            default:
                // func_global_asm_80705C00(0, 0x7530, 0U);
                dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 4, 4, 0, -1, 0.0f);
                break;
            }
            break;
        case MAP_JAPES_ARMY_DILLO:
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 3, 3, 1, -1, 0.0f);
            // dl = func_global_asm_80705F5C(dl, 0, 0x7D00, 0);
            break;
        case MAP_FUNGI:
            if ((character_change_array->chunk >= 0xC) && (character_change_array->chunk < 0x12)) {
                // dl = func_global_asm_8070770C(dl);
            } else {
                // func_global_asm_80705C00(0, 0x5DC0, 1U);
                dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 2, 2, 1, -1, 0.0f);
                // dl = func_global_asm_80705F5C(dl, 0, 0x5DC0, 1);
            }
            break;
        case MAP_FUNGI_MINECART:
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 1, 1, 1, -1, 0.0f);
            break;
        case MAP_DK_ISLES_OVERWORLD:
            if (func_global_asm_8061CB50() != 0) {
                var_f14 = character_change_array->look_at_eye[0];
                var_f16 = character_change_array->look_at_eye[2];
            } else {
                var_f14 = gPlayerPointer->position.f[0];
                var_f16 = gPlayerPointer->position.f[2];
            }
            temp_f0 = var_f14 - 3000.0f;
            temp_f2 = var_f16 - 5000.0f;
            temp_f0 = sqrtf(SQ(temp_f0) + SQ(temp_f2));
            if (temp_f0 < 2500.0f) {
                temp_f12 = (f64) ((2500.0f - temp_f0) / 1000.0f);
                var_f2 = MIN(1.0, temp_f12);
                func_global_asm_8068B830((s16) (s32) (2.0f + var_f2), (s16) (s32) (var_f2 * 400.0f), (s16) (s32) (var_f2 * 30.0f));
                func_global_asm_8068B8A4((f32) (((f64) var_f2 * -0.6) + 1.0));
            } else {
                func_global_asm_8068B8FC();
                func_global_asm_8068B8A4(1.0f);
            }
            // func_global_asm_80705C00(0xBB8, 0x4E20, 0U);
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 5, 6, 1, -1, 0.0f);
            break;
        case MAP_DK_ISLES_DK_THEATRE:
        case MAP_ROCK_INTRO_STORY:
            // func_global_asm_80705C00(0xBB8, 0x4E20, 0U);
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 5, 5, 1, -1, 0.0f);
            break;
        case MAP_GALLEON_PUFFTOSS:
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 6, 6, 1, -1, 0.0f);
            // dl = func_global_asm_80705F5C(dl, 0xC8, 0x4268, 0);
            break;
        case MAP_CASTLE:
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 6, 6, 1, -1, 0.0f);
            // dl = func_global_asm_80705F5C(dl, 0x3E8, 0x2EE0, 0);
            break;
        case MAP_KLUMSY_ENDING:
            // func_global_asm_80705C00(0x8FC, 0x1388, 0U);
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 7, 7, 0, -1, 0.0f);
            break;
        case MAP_BLOOPERS_ENDING:
            gDPSetFillColor(dl++, 0xFFFFFFFF);
            goto block_55;
        case MAP_GALLEON_BARREL_BLAST:
            gDPSetFillColor(dl++, 0xFFC1FFC1);
            goto block_55;
        case MAP_MAIN_MENU:
            func_global_asm_80705C00(0x8FC, 0x1388, 1U);
            dl = func_global_asm_80704B20(dl, arg1, arg2, arg3, 7, 6, 0, -1, D_global_asm_80754CE8);
            dl = func_global_asm_80705F5C(dl, 0xC8, 0x2EE0, 2);
            break;
            
        default:
            gDPSetFillColor(dl++, 0x00010001);
    block_55:
            gDPSetRenderMode(dl++, G_RM_NOOP, G_RM_NOOP2);
            gDPSetCycleType(dl++, G_CYC_FILL);
            // @recomp: remove the -1
            gDPFillRectangle(dl++,
                character_change_array[arg4].unk270[0],
                character_change_array[arg4].unk270[1],
                character_change_array[arg4].unk270[2],
                character_change_array[arg4].unk270[3]
            );
            gDPPipeSync(dl++);
            gDPSetCycleType(dl++, G_CYC_1CYCLE);
            break;
    }
    if (impactedByDayNight()) {
        if (cycle_time < (0.5f + SUNSET_RANGE)) {
            // Partially night
            dl = func_global_asm_80705F5C(dl,
                stage == 1 ? 3000 : 952,
                32767 * ((1.0f - cycle_time) - (0.5f - SUNSET_RANGE)) * (1.0f / (0.5f + SUNSET_RANGE)),
                0);
        }
    }
    D_global_asm_80754CE8 = 0.0f;
    gDPPipeSync(dl++);
    return dl;
}