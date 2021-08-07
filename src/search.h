#ifndef SEARCH_H
#define SEARCH_H

#include "cubiomes/finders.h"

#include <atomic>
#include <vector>
#include <QString>
#include <QObject>

#define PRECOMPUTE48_BUFSIZ ((int64_t)1 << 30)

enum
{
    CAT_NONE,
    CAT_QUAD,
    CAT_STRUCT,
    CAT_BIOMES,
    CAT_NETHER,
    CAT_END,
    CAT_OTHER,
    CATEGORY_MAX,
};

struct FilterInfo
{
    int cat;    // seed source category
    bool coord; // requires coordinate entry
    bool area;  // requires area entry
    int layer;  // associated generator layer
    int stype;  // structure type
    int step;   // coordinate multiplier
    int count;  //
    int mcmin;  // minimum version
    int dim;    // dimension
    int disp;   // display order
    const char *icon;
    QString name;
    QString description;
};

enum
{
    F_SELECT,
    F_QH_IDEAL,
    F_QH_CLASSIC,
    F_QH_NORMAL,
    F_QH_BARELY,
    F_QM_95,
    F_QM_90,
    F_BIOME,
    F_BIOME_4_RIVER,
    F_BIOME_16_SHORE,
    F_BIOME_64_RARE,
    F_BIOME_256_BIOME,
    F_BIOME_256_OTEMP,
    F_TEMPS,
    F_SLIME,
    F_SPAWN,
    F_STRONGHOLD,
    F_DESERT,
    F_JUNGLE,
    F_HUT,
    F_IGLOO,
    F_MONUMENT,
    F_VILLAGE,
    F_OUTPOST,
    F_MANSION,
    F_TREASURE,
    F_RUINS,
    F_SHIPWRECK,
    F_PORTAL,
    F_FORTRESS,
    F_BASTION,
    F_ENDCITY,
    F_BIOME_NETHER_1,
    F_BIOME_NETHER_4,
    F_BIOME_NETHER_16,
    F_BIOME_NETHER_64,
    F_BIOME_END_1,
    F_BIOME_END_4,
    F_BIOME_END_16,
    F_BIOME_END_64,
    F_PORTALN,
    F_GATEWAY,
    F_MINESHAFT,
    // new filters should be added here at the end to keep some downwards compatibility
    FILTER_MAX,
};

// global table of filter data (as constants with enum indexing)
static const struct FilterList
{
    FilterInfo list[FILTER_MAX];

    FilterList() : list{}
    {
        int disp = 0; // display order

        list[F_SELECT] = FilterInfo{
            CAT_NONE, 0, 0, 0, 0, 0, 0, MC_1_0, 0, disp++,
            NULL,
            "",
            ""
        };

        list[F_QH_IDEAL] = FilterInfo{
            CAT_QUAD, 1, 1, 0, Swamp_Hut, 512, 0, MC_1_4, 0, disp++,
            ":icons/quad.png",
            QObject::tr("Quad-hut (ideal)"),
            QObject::tr("The lower 48-bits provide potential for four swamp huts in "
            "spawning range, in one of the best configurations that exist.")
        };

        list[F_QH_CLASSIC] = FilterInfo{
            CAT_QUAD, 1, 1, 0, Swamp_Hut, 512, 0, MC_1_4, 0, disp++,
            ":icons/quad.png",
            QObject::tr("Quad-hut (classic)"),
            QObject::tr("The lower 48-bits provide potential for four swamp huts in "
            "spawning range, in one of the \"classic\" configurations. "
            "(Checks for huts in the nearest 2x2 chunk corners of each "
            "region.)")
        };

        list[F_QH_NORMAL] = FilterInfo{
            CAT_QUAD, 1, 1, 0, Swamp_Hut, 512, 0, MC_1_4, 0, disp++,
            ":icons/quad.png",
            QObject::tr("Quad-hut (normal)"),
            QObject::tr("The lower 48-bits provide potential for four swamp huts in "
            "spawning range, such that all of them are within 128 blocks "
            "of a single AFK location, including a vertical tollerance "
            "for a fall damage chute.")
        };

        list[F_QH_BARELY] = FilterInfo{
            CAT_QUAD, 1, 1, 0, Swamp_Hut, 512, 0, MC_1_4, 0, disp++,
            ":icons/quad.png",
            QObject::tr("Quad-hut (barely)"),
            QObject::tr("The lower 48-bits provide potential for four swamp huts in "
            "spawning range, in any configuration, such that the bounding "
            "boxes are within 128 blocks of a single AFK location.")
        };

        list[F_QM_95] = FilterInfo{
            CAT_QUAD, 1, 1, 0, Monument, 512, 0, MC_1_8, 0, disp++,
            ":icons/quad.png",
            QObject::tr("Quad-ocean-monument (>95%)"),
            QObject::tr("The lower 48-bits provide potential for 95% of the area of "
            "four ocean monuments to be within 128 blocks of an AFK "
            "location.")
        };

        list[F_QM_90] = FilterInfo{
            CAT_QUAD, 1, 1, 0, Monument, 512, 0, MC_1_8, 0, disp++,
            ":icons/quad.png",
            QObject::tr("Quad-ocean-monument (>90%)"),
            QObject::tr("The lower 48-bits provide potential for 90% of the area of "
            "four ocean monuments to be within 128 blocks of an AFK "
            "location.")
        };

        list[F_BIOME] = FilterInfo{
            CAT_BIOMES, 1, 1, L_VORONOI_1, 0, 1, 0, MC_1_0, 0, disp++,
            ":icons/map.png",
            QObject::tr("Biome filter 1:1"),
            QObject::tr("Only seeds with the included (+) biomes in the specified area and "
            "discard those that have biomes that are explicitly excluded (-).")
        };

        list[F_BIOME_4_RIVER] = FilterInfo{
            CAT_BIOMES, 1, 1, L_RIVER_MIX_4, 0, 4, 0, MC_1_0, 0, disp++,
            ":icons/map.png",
            QObject::tr("Biome filter 1:4 RIVER"),
            QObject::tr("Only seeds with the included (+) biomes in the specified area and "
            "discard those that have biomes that are explicitly excluded (-) "
            "at layer RIVER with scale 1:4.")
        };

        list[F_BIOME_16_SHORE] = FilterInfo{
            CAT_BIOMES, 1, 1, L_SHORE_16, 0, 16, 0, MC_1_1, 0, disp++,
            ":icons/map.png",
            QObject::tr("Biome filter 1:16 SHORE"),
            QObject::tr("Only seeds with the included (+) biomes in the specified area and "
            "discard those that have biomes that are explicitly excluded (-) "
            "at layer SHORE with scale 1:16.")
        };

        list[F_BIOME_64_RARE] = FilterInfo{
            CAT_BIOMES, 1, 1, L_SUNFLOWER_64, 0, 64, 0, MC_1_7, 0, disp++,
            ":icons/map.png",
            QObject::tr("Biome filter 1:64 RARE"),
            QObject::tr("Only seeds with the included (+) biomes in the specified area and "
            "discard those that have biomes that are explicitly excluded (-) "
            "at layer RARE/SUNFLOWER with scale 1:64.")
        };

        list[F_BIOME_256_BIOME] = FilterInfo{
            CAT_BIOMES, 1, 1, L_BIOME_256, 0, 256, 0, MC_1_0, 0, disp++,
            ":icons/map.png",
            QObject::tr("Biome filter 1:256 BIOME"),
            QObject::tr("Only seeds with the included (+) biomes in the specified area and "
            "discard those that have biomes that are explicitly excluded (-) "
            "at layer BIOME with scale 1:256.")
        };

        list[F_BIOME_256_OTEMP] = FilterInfo{
            CAT_BIOMES, 1, 1, L_OCEAN_TEMP_256, 0, 256, 0, MC_1_13, 0, disp++,
            ":icons/map.png",
            QObject::tr("Biome filter 1:256 O.TEMP"),
            QObject::tr("Only seeds with the included (+) biomes in the specified area and "
            "discard those that have biomes that are explicitly excluded (-) "
            "at layer OCEAN TEMPERATURE with scale 1:256. "
            "This generation layer depends only on the lower 48-bits of the seed.")
        };

        list[F_TEMPS] = FilterInfo{
            CAT_BIOMES, 1, 1, 0, 0, 1024, 0, MC_1_7, 0, disp++,
            ":icons/tempcat.png",
            QObject::tr("Temperature categories"),
            QObject::tr("Checks that the area has a minimum of all the required temperature categories.")
        };

        list[F_BIOME_NETHER_1] = FilterInfo{
            CAT_NETHER, 1, 1, 0, 0, 1, 0, MC_1_16, -1, disp++,
            ":icons/nether.png",
            QObject::tr("Nether biome filter 1:1"),
            QObject::tr("Nether biomes after voronoi scaling to 1:1. (height: y = 0)")
        };
        list[F_BIOME_NETHER_4] = FilterInfo{
            CAT_NETHER, 1, 1, 0, 0, 4, 0, MC_1_16, -1, disp++,
            ":icons/nether.png",
            QObject::tr("Nether biome filter 1:4"),
            QObject::tr("Nether biomes with normal noise sampling at scale 1:4. (height: y = 0) "
            "(The Nether and End depend only on the lower 48-bits of the seed.)")
        };
        list[F_BIOME_NETHER_16] = FilterInfo{
            CAT_NETHER, 1, 1, 0, 0, 16, 0, MC_1_16, -1, disp++,
            ":icons/nether.png",
            QObject::tr("Nether biome filter 1:16"),
            QObject::tr("Nether biomes, but only sampled at scale 1:16. (height: y = 0) "
            "(The Nether and End depend only on the lower 48-bits of the seed.)")
        };
        list[F_BIOME_NETHER_64] = FilterInfo{
            CAT_NETHER, 1, 1, 0, 0, 64, 0, MC_1_16, -1, disp++,
            ":icons/nether.png",
            QObject::tr("Nether biome filter 1:64"),
            QObject::tr("Nether biomes, but only sampled at scale 1:64, (height: y = 0) "
            "(The Nether and End depend only on the lower 48-bits of the seed.)")
        };

        list[F_BIOME_END_1] = FilterInfo{
            CAT_END, 1, 1, 0, 0, 1, 0, MC_1_9, +1, disp++,
            ":icons/the_end.png",
            QObject::tr("End biome filter 1:1"),
            QObject::tr("End biomes after voronoi scaling to 1:1.")
        };
        list[F_BIOME_END_4] = FilterInfo{
            CAT_END, 1, 1, 0, 0, 4, 0, MC_1_9, +1, disp++,
            ":icons/the_end.png",
            QObject::tr("End biome filter 1:4"),
            QObject::tr("End biomes sampled at scale 1:4. Note this is just a simple upscale of 1:16. "
            "(The Nether and End depend only on the lower 48-bits of the seed.)")
        };
        list[F_BIOME_END_16] = FilterInfo{
            CAT_END, 1, 1, 0, 0, 16, 0, MC_1_9, +1, disp++,
            ":icons/the_end.png",
            QObject::tr("End biome filter 1:16"),
            QObject::tr("End biomes with normal sampling at scale 1:16. "
            "(The Nether and End depend only on the lower 48-bits of the seed.)")
        };
        list[F_BIOME_END_64] = FilterInfo{
            CAT_END, 1, 1, 0, 0, 64, 0, MC_1_9, +1, disp++,
            ":icons/the_end.png",
            QObject::tr("End biome filter 1:64"),
            QObject::tr("End biomes with lossy sampling at scale 1:64. "
            "(The Nether and End depend only on the lower 48-bits of the seed.)")
        };

        list[F_SPAWN] = FilterInfo{
            CAT_OTHER, 1, 1, 0, 0, 1, 0, MC_1_0, 0, disp++,
            ":icons/spawn.png",
            QObject::tr("Spawn"),
            ""
        };

        list[F_SLIME] = FilterInfo{
            CAT_OTHER, 1, 1, 0, 0, 16, 1, MC_1_0, 0, disp++,
            ":icons/slime.png",
            QObject::tr("Slime chunk"),
            ""
        };

        list[F_STRONGHOLD] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, 0, 1, 1, MC_1_0, 0, disp++,
            ":icons/stronghold.png",
            QObject::tr("Stronghold"),
            ""
        };

        list[F_VILLAGE] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Village, 1, 1, MC_1_0, 0, disp++,
            ":icons/village.png",
            QObject::tr("Village"),
            ""
        };

        list[F_MINESHAFT] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Mineshaft, 1, 1, MC_1_0, 0, disp++,
            ":icons/mineshaft.png",
            QObject::tr("Abandoned mineshaft"),
            ""
        };

        list[F_DESERT] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Desert_Pyramid, 1, 1, MC_1_3, 0, disp++,
            ":icons/desert.png",
            QObject::tr("Desert pyramid"),
            ""
        };

        list[F_JUNGLE] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Jungle_Pyramid, 1, 1, MC_1_3, 0, disp++,
            ":icons/jungle.png",
            QObject::tr("Jungle temple"),
            ""
        };

        list[F_HUT] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Swamp_Hut, 1, 1, MC_1_4, 0, disp++,
            ":icons/hut.png",
            QObject::tr("Swamp hut"),
            ""
        };

        list[F_MONUMENT] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Monument, 1, 1, MC_1_8, 0, disp++,
            ":icons/monument.png",
            QObject::tr("Ocean monument"),
            ""
        };

        list[F_IGLOO] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Igloo, 1, 1, MC_1_9, 0, disp++,
            ":icons/igloo.png",
            QObject::tr("Igloo"),
            ""
        };

        list[F_MANSION] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Mansion, 1, 1, MC_1_11, 0, disp++,
            ":icons/mansion.png",
            QObject::tr("Woodland mansion"),
            ""
        };

        list[F_RUINS] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Ocean_Ruin, 1, 1, MC_1_13, 0, disp++,
            ":icons/ruins.png",
            QObject::tr("Ocean ruins"),
            ""
        };

        list[F_SHIPWRECK] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Shipwreck, 1, 1, MC_1_13, 0, disp++,
            ":icons/shipwreck.png",
            QObject::tr("Shipwreck"),
            ""
        };

        list[F_TREASURE] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Treasure, 1, 1, MC_1_13, 0, disp++,
            ":icons/treasure.png",
            QObject::tr("Buried treasure"),
            ""
        };

        list[F_OUTPOST] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Outpost, 1, 1, MC_1_14, 0, disp++,
            ":icons/outpost.png",
            QObject::tr("Pillager outpost"),
            ""
        };

        list[F_PORTAL] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Ruined_Portal, 1, 1, MC_1_16, 0, disp++,
            ":icons/portal.png",
            QObject::tr("Ruined portal (overworld)"),
            ""
        };

        list[F_PORTALN] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Ruined_Portal_N, 1, 1, MC_1_16, -1, disp++,
            ":icons/portal.png",
            QObject::tr("Ruined portal (nether)"),
            QObject::tr("(The Nether and End depend only on the lower 48-bits of the seed.)")
        };

        list[F_FORTRESS] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Fortress, 1, 1, MC_1_0, -1, disp++,
            ":icons/fortress.png",
            QObject::tr("Nether fortress"),
            QObject::tr("(The Nether and End depend only on the lower 48-bits of the seed.)")
        };

        list[F_BASTION] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, Bastion, 1, 1, MC_1_16, -1, disp++,
            ":icons/bastion.png",
            QObject::tr("Bastion remnant"),
            QObject::tr("(The Nether and End depend only on the lower 48-bits of the seed.)")
        };

        list[F_ENDCITY] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, End_City, 1, 1, MC_1_9, +1, disp++,
            ":icons/endcity.png",
            QObject::tr("End city"),
            QObject::tr("(The Nether and End depend only on the lower 48-bits of the seed.)")
        };

        list[F_GATEWAY] = FilterInfo{
            CAT_STRUCT, 1, 1, 0, End_Gateway, 1, 1, MC_1_13, +1, disp++,
            ":icons/gateway.png",
            QObject::tr("End gateway"),
            QObject::tr("Scattered end gateway return portals, not including those "
            "generated when defeating the dragon. "
            "(The Nether and End depend only on the lower 48-bits of the seed.)")
        };
    }
}
g_filterinfo;


struct Condition
{
    int type;
    int x1, z1, x2, z2;
    int save;
    int relative;
    BiomeFilter bfilter;
    uint64_t exclb; // excluded biome
    uint64_t exclm; // excluded modified
    int temps[9];
    int count;
};

struct StructPos
{
    StructureConfig sconf;
    int cx, cz; // effective center position
};

int testCond(
        StructPos *spos, const Condition *cond,
        int mc, LayerStack *g, uint64_t seed, std::atomic_bool *abort);

struct QuadInfo
{
    Pos p[4];   // individual positions
    Pos afk;    // optimal afk position
    int typ;    // type of structure
    int spcnt;  // number of planar spawning spaces
    float rad;  // enclosing radius
};

void findQuadStructs(
        int styp, int mc, LayerStack *g, uint64_t seed,
        std::vector<QuadInfo> *out);


#endif // SEARCH_H
