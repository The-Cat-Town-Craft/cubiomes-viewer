#include "quad.h"

#include "cutil.h"

#include <QThreadPool>

#include <cmath>
#include <algorithm>

#define SEAM_BUF 8


Quad::Quad(const Level* l, int i, int j)
    : wi(l->wi),dim(l->dim),entry(l->entry)
    , ti(i),tj(j),blocks(l->blocks),pixs(l->pixs),sopt(l->sopt)
    , rgb(),img(),spos()
    , done()
    , prio(),stopped()
{
    setAutoDelete(false);
}

Quad::~Quad()
{
    delete [] rgb;
    delete img;
    delete spos;
}

void getStructs(std::vector<VarPos> *out, const StructureConfig sconf,
        WorldInfo wi, int dim, int x0, int z0, int x1, int z1)
{
    union {
        LayerStack g;
        NetherNoise nn;
        EndNoise en;
    } u;

    int si0 = (int)floor(x0 / (qreal)(sconf.regionSize * 16));
    int sj0 = (int)floor(z0 / (qreal)(sconf.regionSize * 16));
    int si1 = (int)floor((x1-1) / (qreal)(sconf.regionSize * 16));
    int sj1 = (int)floor((z1-1) / (qreal)(sconf.regionSize * 16));

    if (dim == 0)
    {
        setupGeneratorLargeBiomes(&u.g, wi.mc, wi.large);
    }

    for (int i = si0; i <= si1; i++)
    {
        for (int j = sj0; j <= sj1; j++)
        {
            Pos p;
            if (!getStructurePos(sconf.structType, wi.mc, wi.seed, i, j, &p))
                continue;

            if (p.x >= x0 && p.x < x1 && p.z >= z0 && p.z < z1)
            {
                int id = 0;
                if (dim == 0)
                {
                    id = isViableStructurePos(sconf.structType, wi.mc, &u.g, wi.seed, p.x, p.z);
                }
                else if (dim == -1)
                {
                    id = isViableNetherStructurePos(sconf.structType, wi.mc, &u.nn, wi.seed, p.x, p.z);
                }
                else if (dim == +1)
                {
                    id = isViableEndStructurePos(sconf.structType, wi.mc, &u.en, wi.seed, p.x, p.z);
                    if (id && sconf.structType == End_City)
                    {
                        SurfaceNoise sn;
                        initSurfaceNoiseEnd(&sn, wi.seed);
                        id = isViableEndCityTerrain(&u.en, &sn, p.x, p.z);
                    }
                }

                if (id)
                {
                    VarPos vp = { p, 0 };
                    if (sconf.structType == Village)
                    {
                        VillageType vt = getVillageType(wi.mc, wi.seed, p.x, p.z, id);
                        vp.variant = vt.abandoned;
                    }
                    out->push_back(vp);
                }
            }
        }
    }
}



void Quad::run()
{
    if (done)
        return;

    if (pixs > 0)
    {
        int x = ti*pixs, z = tj*pixs, w = pixs+SEAM_BUF, h = pixs+SEAM_BUF;
        int *b = NULL;
        if (dim == -1)
        {
            b = (int*) malloc((w+7) * (h+7) * sizeof(int));
            genNetherScaled(wi.mc, wi.seed, blocks / pixs, b, x, z, w, h, 0, 0);
        }
        else if (dim == +1) // end
        {
            b = (int*) malloc((w+7) * (h+7) * sizeof(int));
            genEndScaled(wi.mc, wi.seed, blocks / pixs, b, x, z, w, h);
        }
        else
        {
            b = allocCache(entry, w, h);
            genArea(entry, b, x, z, w, h);
        }

        rgb = new uchar[w*h * 3];
        biomesToImage(rgb, biomeColors, b, w, h, 1, 1);
        img = new QImage(rgb, w, h, 3*w, QImage::Format_RGB888);
        free(b);
    }
    else
    {
        int structureType = mapopt2stype(sopt);
        if (structureType >= 0)
        {
            int x0 = ti*blocks, x1 = (ti+1)*blocks;
            int z0 = tj*blocks, z1 = (tj+1)*blocks;
            std::vector<VarPos>* st = new std::vector<VarPos>();
            StructureConfig sconf;
            if (getStructureConfig_override(structureType, wi.mc, &sconf))
                getStructs(st, sconf, wi, dim, x0, z0, x1, z1);
            spos = st;
        }
    }
    done = true;
}


Level::Level()
    : cells(),g(),entry(),wi(),dim()
    , tx(),tz(),tw(),th()
    , scale(),blocks(),pixs()
    , sopt()
{
}

Level::~Level()
{
    QThreadPool::globalInstance()->waitForDone();
    for (Quad *q : cells)
        delete q;
}

int mapOceanMixMod(const Layer * l, int * out, int x, int z, int w, int h)
{
    int *otyp;
    int i, j;
    l->p2->getMap(l->p2, out, x, z, w, h);

    otyp = (int *) malloc(w*h*sizeof(int));
    memcpy(otyp, out, w*h*sizeof(int));

    l->p->getMap(l->p, out, x, z, w, h);


    for (j = 0; j < h; j++)
    {
        for (i = 0; i < w; i++)
        {
            int landID, oceanID;

            landID = out[i + j*w];

            if (!isOceanic(landID))
                continue;

            oceanID = otyp[i + j*w];

            if (landID == deep_ocean)
            {
                switch (oceanID)
                {
                case lukewarm_ocean:
                    oceanID = deep_lukewarm_ocean;
                    break;
                case ocean:
                    oceanID = deep_ocean;
                    break;
                case cold_ocean:
                    oceanID = deep_cold_ocean;
                    break;
                case frozen_ocean:
                    oceanID = deep_frozen_ocean;
                    break;
                }
            }

            out[i + j*w] = oceanID;
        }
    }

    free(otyp);

    return 0;
}

void Level::init4map(WorldInfo wi, int dim, int pix, int layerscale)
{
    this->wi = wi;
    this->dim = dim;

    tx = tz = tw = th = 0;
    scale = layerscale;
    pixs = pix;
    blocks = pix * layerscale;
    sopt = D_NONE;

    if (dim == 0) // overworld
    {
        setupGeneratorLargeBiomes(&g, wi.mc, wi.large);
        int l1 = 0, l2 = 0;
        entry = NULL;

        switch (scale)
        {
        case 1:
            entry = g.entry_1;
            break;
        case 4:
            entry = g.entry_4;
            break;
        case 16:
            if (wi.mc >= MC_1_13) {
                l1 = wi.large ? L_ZOOM_4 : L_SHORE_16;
                l2 = L_ZOOM_16_OCEAN;
            } else {
                entry = g.entry_16;
            }
            break;
        case 64:
            if (wi.mc >= MC_1_13) {
                l1 = wi.large ? L_SHORE_16 : L_SUNFLOWER_64;
                l2 = L_ZOOM_64_OCEAN;
            } else {
                entry = g.entry_64;
            }
            break;
        case 256:
            if (wi.mc >= MC_1_13) {
                if (wi.large) {
                    l1 = L_SUNFLOWER_64;
                } else {
                    l1 = (wi.mc >= MC_1_14 ? L_BAMBOO_256 : L_BIOME_256);
                }
                l2 = L_OCEAN_TEMP_256;
            } else {
                entry = g.entry_256;
            }
            break;
        }
        if (!entry && l1 && l2)
        {
            // setup a custom layer in place of voronoi
            entry = setupLayer(
                &g, L_VORONOI_1, &mapOceanMixMod, wi.mc, 1, 0, 0,
                &g.layers[l1], &g.layers[l2]);
        }
        if (!entry)
        {
            printf("Bad scale (%d) for level\n", scale);
            exit(1);
        }

        setLayerSeed(entry, wi.seed);
    }
}

void Level::init4struct(WorldInfo wi, int dim, int blocks, int sopt, int lv)
{
    this->wi = wi;
    this->dim = dim;
    this->blocks = blocks;
    this->pixs = -1;
    this->scale = -1;
    this->sopt = sopt;
    this->viewlv = lv;
}

static int sqdist(int x, int z) { return x*x + z*z; }

void Level::resizeLevel(std::vector<Quad*>& cache, int x, int z, int w, int h)
{
    // move the cells from the old grid to the new grid
    // or to the cached queue if they are not inside the new grid
    std::vector<Quad*> grid(w*h);
    std::vector<Quad*> togen;

    for (Quad *q : cells)
    {
        int gx = q->ti - x;
        int gz = q->tj - z;
        if (gx >= 0 && gx < w && gz >= 0 && gz < h)
            grid[gz*w + gx] = q;
        else
            cache.push_back(q);
    }

    // look through the cached queue for reusable quads
    std::vector<Quad*> newcache;
    for (Quad *c : cache)
    {
        int gx = c->ti - x;
        int gz = c->tj - z;

        if (c->blocks == blocks && c->sopt == sopt && c->dim == dim)
        {
            // remove outside quads from schedule
            if (QThreadPool::globalInstance()->tryTake(c))
            {
                c->stopped = true;
            }

            if (gx >= 0 && gx < w && gz >= 0 && gz < h)
            {
                Quad *& g = grid[gz*w + gx];
                if (g == NULL)
                {
                    g = c;
                    continue;
                }
            }
        }
        newcache.push_back(c);
    }
    cache.swap(newcache);

    // collect which quads need generation and add any that are missing
    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            Quad *& g = grid[j*w + i];
            if (g == NULL)
            {
                g = new Quad(this, x+i, z+j);
                g->prio = sqdist(i-w/2, j-h/2);
                togen.push_back(g);
            }
            else if (g->stopped || QThreadPool::globalInstance()->tryTake(g))
            {
                if (!g->done)
                {
                    g->stopped = false;
                    g->prio = sqdist(i-w/2, j-h/2);
                    if (g->dim != dim)
                        g->prio += 1000000;
                    togen.push_back(g);
                }
            }
        }
    }

    // start the quad processing
    std::sort(togen.begin(), togen.end(),
              [](Quad* a, Quad* b) { return a->prio < b->prio; });
    for (Quad *q : togen)
        QThreadPool::globalInstance()->start(q, scale);

    cells.swap(grid);
    tx = x;
    tz = z;
    tw = w;
    th = h;
}

void Level::update(std::vector<Quad*>& cache, qreal bx0, qreal bz0, qreal bx1, qreal bz1)
{
    int nti0 = (int) std::floor(bx0 / blocks);
    int ntj0 = (int) std::floor(bz0 / blocks);
    int nti1 = (int) std::floor(bx1 / blocks) + 1;
    int ntj1 = (int) std::floor(bz1 / blocks) + 1;

    // resize if the new area is much smaller or in an unprocessed range
    if ((nti1-nti0)*2 < tw || nti0 < tx || nti1 > tx+tw || ntj0 < tz || ntj1 > tz+th)
    {
        qreal padf = 0.2 * (bx1 - bx0);
        nti0 = (int) std::floor((bx0-padf) / blocks);
        ntj0 = (int) std::floor((bz0-padf) / blocks);
        nti1 = (int) std::floor((bx1+padf) / blocks) + 1;
        ntj1 = (int) std::floor((bz1+padf) / blocks) + 1;

        resizeLevel(cache, nti0, ntj0, nti1-nti0, ntj1-ntj0);
    }
}


QWorld::QWorld(WorldInfo wi, int dim)
    : wi(wi)
    , dim(dim)
    , sha()
    , lvb()
    , lvs()
    , activelv()
    , cachedbiomes()
    , cachedstruct()
    , cachesize()
    , showBB()
    , spawn()
    , strongholds()
    , qsinfo()
    , isdel()
    , slimeimg()
    , slimex()
    , slimez()
    , seldo()
    , selx()
    , selz()
    , seltype(-1)
    , selpos()
    , selvar()
    , qual()
{
    setupGeneratorLargeBiomes(&g, wi.mc, wi.large);
    applySeed(&g, wi.seed);
    sha = getVoronoiSHA(wi.seed);

    activelv = 0;

    int pixs = 512;
    lvs.resize(D_SPAWN);
    lvs[D_DESERT]       .init4struct(wi, 0, 2048, D_DESERT, 2);
    lvs[D_JUNGLE]       .init4struct(wi, 0, 2048, D_JUNGLE, 2);
    lvs[D_IGLOO]        .init4struct(wi, 0, 2048, D_IGLOO, 2);
    lvs[D_HUT]          .init4struct(wi, 0, 2048, D_HUT, 2);
    lvs[D_VILLAGE]      .init4struct(wi, 0, 2048, D_VILLAGE, 2);
    lvs[D_MANSION]      .init4struct(wi, 0, 2048, D_MANSION, 3);
    lvs[D_MONUMENT]     .init4struct(wi, 0, 2048, D_MONUMENT, 2);
    lvs[D_RUINS]        .init4struct(wi, 0, 2048, D_RUINS, 1);
    lvs[D_SHIPWRECK]    .init4struct(wi, 0, 2048, D_SHIPWRECK, 1);
    lvs[D_TREASURE]     .init4struct(wi, 0, 2048, D_TREASURE, 1);
    lvs[D_OUTPOST]      .init4struct(wi, 0, 2048, D_OUTPOST, 2);
    lvs[D_PORTAL]       .init4struct(wi, 0, 2048, D_PORTAL, 1);
    lvs[D_PORTALN]      .init4struct(wi,-1, 2048, D_PORTALN, 1);
    lvs[D_FORTESS]      .init4struct(wi,-1, 2048, D_FORTESS, 1);
    lvs[D_BASTION]      .init4struct(wi,-1, 2048, D_BASTION, 1);
    lvs[D_ENDCITY]      .init4struct(wi, 1, 2048, D_ENDCITY, 2);
    lvs[D_GATEWAY]      .init4struct(wi, 1, 2048, D_GATEWAY, 2);
    lvs[D_MINESHAFT]    .init4struct(wi, 0, 2048, D_MINESHAFT, 1);

    if (dim == 0)
    {
        lvb.resize(5);
        lvb[0].init4map(wi, dim, pixs, 1);
        lvb[1].init4map(wi, dim, pixs, 4);
        lvb[2].init4map(wi, dim, pixs, 16);
        lvb[3].init4map(wi, dim, pixs, 64);
        lvb[4].init4map(wi, dim, pixs, 256);
    }
    else
    {
        lvb.resize(4);
        lvb[0].init4map(wi, dim, pixs, 1);
        lvb[1].init4map(wi, dim, pixs, 4);
        lvb[2].init4map(wi, dim, pixs, 16);
        lvb[3].init4map(wi, dim, pixs, 64);
    }
    cachesize = 100;
    qual = 1.0;

    memset(sshow, 0, sizeof(sshow));

    icons[D_DESERT]     = QPixmap(":/icons/desert.png");
    icons[D_JUNGLE]     = QPixmap(":/icons/jungle.png");
    icons[D_IGLOO]      = QPixmap(":/icons/igloo.png");
    icons[D_HUT]        = QPixmap(":/icons/hut.png");
    icons[D_VILLAGE]    = QPixmap(":/icons/village.png");
    icons[D_MANSION]    = QPixmap(":/icons/mansion.png");
    icons[D_MONUMENT]   = QPixmap(":/icons/monument.png");
    icons[D_RUINS]      = QPixmap(":/icons/ruins.png");
    icons[D_SHIPWRECK]  = QPixmap(":/icons/shipwreck.png");
    icons[D_TREASURE]   = QPixmap(":/icons/treasure.png");
    icons[D_MINESHAFT]  = QPixmap(":/icons/mineshaft.png");
    icons[D_OUTPOST]    = QPixmap(":/icons/outpost.png");
    icons[D_PORTAL]     = QPixmap(":/icons/portal.png");
    icons[D_PORTALN]    = QPixmap(":/icons/portal.png");
    icons[D_SPAWN]      = QPixmap(":/icons/spawn.png");
    icons[D_STRONGHOLD] = QPixmap(":/icons/stronghold.png");
    icons[D_FORTESS]    = QPixmap(":/icons/fortress.png");
    icons[D_BASTION]    = QPixmap(":/icons/bastion.png");
    icons[D_ENDCITY]    = QPixmap(":/icons/endcity.png");
    icons[D_GATEWAY]    = QPixmap(":/icons/gateway.png");

    iconzvil = QPixmap(":/icons/zombie.png");
}

QWorld::~QWorld()
{
    isdel = true;
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone();
    for (Quad *q : cachedbiomes)
        delete q;
    for (Quad *q : cachedstruct)
        delete q;
    if (spawn && spawn != (Pos*)-1)
    {
        delete spawn;
        delete strongholds;
        delete qsinfo;
    }
}

void QWorld::setDim(int dim)
{
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone();
    this->dim = dim;

    // cache existing quads
    for (Level& l : lvb)
    {
        std::vector<Quad*> todel;
        for (Quad *q : l.cells)
        {
            if (q->done)
                cachedbiomes.push_back(q);
            else
                todel.push_back(q);
        }
        l.cells.swap(todel);
    }

    cleancache(cachedbiomes, (int)(cachesize));

    lvb.clear();
    int pixs = 512;
    if (dim == 0)
    {
        lvb.resize(5);
        lvb[0].init4map(wi, dim, pixs, 1);
        lvb[1].init4map(wi, dim, pixs, 4);
        lvb[2].init4map(wi, dim, pixs, 16);
        lvb[3].init4map(wi, dim, pixs, 64);
        lvb[4].init4map(wi, dim, pixs, 256);
    }
    else
    {
        lvb.resize(4);
        lvb[0].init4map(wi, dim, pixs, 1);
        lvb[1].init4map(wi, dim, pixs, 4);
        lvb[2].init4map(wi, dim, pixs, 16);
        lvb[3].init4map(wi, dim, pixs, 64);
    }
}

int QWorld::getBiome(Pos p)
{
    if (dim == -1)
    {
        if (wi.mc < MC_1_16)
            return nether_wastes;
        NetherNoise nn;
        setNetherSeed(&nn, wi.seed);
        voronoiAccess3D(sha, p.x, 0, p.z, &p.x, 0, &p.z);
        return getNetherBiome(&nn, p.x, 0, p.z, NULL);
    }
    else if (dim == 1)
    {
        if (wi.mc < MC_1_9)
            return the_end;
        int buf[49];
        genEndScaled(wi.mc, wi.seed, 1, buf, p.x, p.z, 1, 1);
        return buf[0];
    }
    return getBiomeAtPos(&g, p);
}

void QWorld::cleancache(std::vector<Quad*>& cache, unsigned int maxsize)
{
    // try to delete the oldest entries in the cache
    if (cache.size() > maxsize)
    {
        std::vector<Quad*> newcache;
        int i;
        for (i = cache.size()-1; i >= 0; --i)
        {
            Quad *q = cache[i];
            if (newcache.size() + i < maxsize * 0.8)
            {
                newcache.push_back(q);
            }
            else
            {
                if (q->done || q->stopped || QThreadPool::globalInstance()->tryTake(q))
                    delete q;
                else
                    newcache.push_back(q);
            }
        }

        cache.resize(newcache.size());
        std::copy(newcache.rbegin(), newcache.rend(), cache.begin());
    }
}


struct SpawnStronghold : public QRunnable
{
    QWorld *world;
    WorldInfo wi;

    SpawnStronghold(QWorld *world, WorldInfo wi) :
        world(world),wi(wi) {}

    void run()
    {
        LayerStack g;
        setupGeneratorLargeBiomes(&g, wi.mc, wi.large);
        applySeed(&g, wi.seed);

        Pos *p = new Pos;
        *p = getSpawn(wi.mc, &g, NULL, wi.seed);
        world->spawn = p;
        if (world->isdel) return;

        StrongholdIter sh;
        initFirstStronghold(&sh, wi.mc, wi.seed);

        std::vector<Pos> *shp = new std::vector<Pos>;
        shp->reserve(wi.mc >= MC_1_9 ? 128 : 3);

        while (nextStronghold(&sh, &g, NULL) > 0)
        {
            if (world->isdel)
            {
                delete shp;
                return;
            }
            shp->push_back(sh.pos);
        }

        world->strongholds = shp;

        std::vector<QuadInfo> *qsinfo = new std::vector<QuadInfo>;

        if (!world->isdel)
            findQuadStructs(Swamp_Hut, wi.mc, &g, wi.seed, qsinfo);
        if (!world->isdel)
            findQuadStructs(Monument, wi.mc, &g, wi.seed, qsinfo);

        world->qsinfo = qsinfo;
    }
};


void QWorld::draw(QPainter& painter, int vw, int vh, qreal focusx, qreal focusz, qreal blocks2pix)
{
    qreal uiw = vw / blocks2pix;
    qreal uih = vh / blocks2pix;

    qreal bx0 = focusx - uiw/2;
    qreal bz0 = focusz - uih/2;
    qreal bx1 = focusx + uiw/2;
    qreal bz1 = focusz + uih/2;

    if      (blocks2pix >= qual)     activelv = -1;
    else if (blocks2pix >= qual/4)   activelv = 0;
    else if (blocks2pix >= qual/16)  activelv = 1;
    else if (blocks2pix >= qual/64)  activelv = 2;
    else if (blocks2pix >= qual/256) activelv = 3;
    else activelv = lvb.size()-1;

    for (int li = activelv+1; li >= activelv; --li)
    {
        if (li < 0 || li >= (int)lvb.size())
            continue;
        Level& l = lvb[li];
        for (Quad *q : l.cells)
        {
            if (!q->img)
                continue;
            // q was processed in another thread and is now done
            qreal ps = q->blocks * blocks2pix;
            qreal px = vw/2.0 + (q->ti) * ps - focusx * blocks2pix;
            qreal pz = vh/2.0 + (q->tj) * ps - focusz * blocks2pix;
            // account for the seam buffer pixels
            ps += ((SEAM_BUF)*q->blocks / (qreal)q->pixs) * blocks2pix;
            QRect rec(px,pz,ps,ps);
            painter.drawImage(rec, *q->img);

            if (sshow[D_GRID])
            {
                QString s = QString::asprintf("%d,%d", q->ti*q->blocks, q->tj*q->blocks);
                QRect textrec = painter.fontMetrics()
                        .boundingRect(rec, Qt::AlignLeft | Qt::AlignTop, s);

                painter.fillRect(textrec, QBrush(QColor(0, 0, 0, 128), Qt::SolidPattern));

                painter.setPen(QColor(255, 255, 255));
                painter.drawText(textrec, s);

                painter.setPen(QPen(QColor(0, 0, 0, 96), 1));
                painter.drawRect(rec);
            }
        }
    }

    if (sshow[D_SLIME] && dim == 0 && blocks2pix*16 > 2.0)
    {
        long x = floor(bx0 / 16), w = floor(bx1 / 16) - x + 1;
        long z = floor(bz0 / 16), h = floor(bz1 / 16) - z + 1;

        // conditions when the slime overlay should be updated
        if (x < slimex || z < slimez ||
            x+w >= slimex+slimeimg.width() || z+h >= slimez+slimeimg.height() ||
            w*h*4 >= slimeimg.width()*slimeimg.height())
        {
            int pad = 64;
            x -= pad;
            z -= pad;
            w += 2*pad;
            h += 2*pad;
            slimeimg = QImage(w, h, QImage::Format_Indexed8);
            slimeimg.setColor(0, qRgba(0, 0, 0, 64));
            slimeimg.setColor(1, qRgba(0, 255, 0, 64));
            slimex = x;
            slimez = z;

            for (int j = 0; j < h; j++)
            {
                for (int i = 0; i < w; i++)
                {
                    int isslime = isSlimeChunk(wi.seed, i+x, j+z);
                    slimeimg.setPixel(i, j, isslime);
                }
            }
        }

        qreal ps = 16 * blocks2pix;
        qreal px = vw/2.0 + slimex * ps - focusx * blocks2pix;
        qreal pz = vh/2.0 + slimez * ps - focusz * blocks2pix;

        QRect rec(px, pz, ps*slimeimg.width(), ps*slimeimg.height());
        painter.drawImage(rec, slimeimg);
    }


    if (showBB && blocks2pix >= 1.0 && qsinfo && dim == 0)
    {
        for (QuadInfo qi : *qsinfo)
        {
            if (qi.typ == Swamp_Hut && !sshow[D_HUT])
                continue;
            if (qi.typ == Monument && !sshow[D_MONUMENT])
                continue;

            qreal x = vw/2.0 + (qi.afk.x - focusx) * blocks2pix;
            qreal y = vh/2.0 + (qi.afk.z - focusz) * blocks2pix;
            qreal r = 128.0 * blocks2pix;
            painter.setPen(QPen(QColor(192, 0, 0, 160), 1));
            painter.drawEllipse(QRectF(x-r, y-r, 2*r, 2*r));
            r = 16;
            painter.drawLine(QPointF(x-r,y), QPointF(x+r,y));
            painter.drawLine(QPointF(x,y-r), QPointF(x,y+r));
        }
    }

    for (int sopt = D_DESERT; sopt < D_SPAWN; sopt++)
    {
        Level& l = lvs[sopt];
        if (!sshow[sopt] || dim != l.dim || activelv > l.viewlv)
            continue;

        std::vector<QPainter::PixmapFragment> frags;

        for (Quad *q : l.cells)
        {
            if (!q->spos)
                continue;
            // q was processed in another thread and is now done
            for (VarPos& vp : *q->spos)
            {
                qreal x = vw/2.0 + (vp.p.x - focusx) * blocks2pix;
                qreal y = vh/2.0 + (vp.p.z - focusz) * blocks2pix;

                if (x < 0 || x >= vw || y < 0 || y >= vh)
                    continue;

                if (showBB && blocks2pix > 1.0)
                {
                    int sx = 0, sz = 0;
                    if (sopt == D_DESERT)
                    {
                        sx = 21; sz = 21;
                    }
                    else if (sopt == D_JUNGLE)
                    {
                        sx = 12; sz = 15;
                    }
                    else if (sopt == D_HUT)
                    {
                        sx = 7; sz = 9;
                    }
                    else if (sopt == D_MONUMENT)
                    {
                        x -= 29 * blocks2pix;
                        y -= 29 * blocks2pix;
                        sx = 58; sz = 58;
                    }

                    if (sx && sz)
                    {   // draw bounding box and move icon to its center
                        qreal dx = sx * blocks2pix;
                        qreal dy = sz * blocks2pix;
                        painter.setPen(QPen(QColor(192, 0, 0, 160), 1));
                        painter.drawRect(QRect(x, y, dx, dy));
                        x += dx / 2;
                        y += dy / 2;
                    }
                }

                QPointF d = QPointF(x, y);

                if (seldo)
                {   // check for structure selection
                    QRectF r = icons[sopt].rect();
                    r.moveCenter(d);
                    if (r.contains(selx, selz))
                    {
                        seltype = sopt;
                        selpos = vp.p;
                        selvar = vp.variant;
                    }
                }
                if (seltype != sopt || selpos.x != vp.p.x || selpos.z != vp.p.z)
                {   // draw unselected structures
                    QRectF r = icons[sopt].rect();
                    if (sopt == D_VILLAGE)
                    {
                        if (vp.variant) {
                            int ix = d.x()-r.width()/2, iy = d.y()-r.height()/2;
                            painter.drawPixmap(ix, iy, iconzvil);
                        } else {
                            frags.push_back(QPainter::PixmapFragment::create(d, r));
                        }
                    }
                    else
                    {
                        frags.push_back(QPainter::PixmapFragment::create(d, r));
                    }
                }
            }
        }

        painter.drawPixmapFragments(frags.data(), frags.size(), icons[sopt]);
    }

    Pos* sp = spawn; // atomic fetch
    if (sp && sp != (Pos*)-1 && sshow[D_SPAWN] && dim == 0)
    {
        qreal x = vw/2.0 + (sp->x - focusx) * blocks2pix;
        qreal y = vh/2.0 + (sp->z - focusz) * blocks2pix;

        QPointF d = QPointF(x, y);
        QRectF r = icons[D_SPAWN].rect();
        painter.drawPixmap(x-r.width()/2, y-r.height()/2, icons[D_SPAWN]);

        if (seldo)
        {
            r.moveCenter(d);
            if (r.contains(selx, selz))
            {
                seltype = D_SPAWN;
                selpos = *sp;
                selvar = 0;
            }
        }
    }

    std::vector<Pos>* shs = strongholds; // atomic fetch
    if (shs && sshow[D_STRONGHOLD] && dim == 0)
    {
        std::vector<QPainter::PixmapFragment> frags;
        frags.reserve(shs->size());
        for (Pos p : *shs)
        {
            qreal x = vw/2.0 + (p.x - focusx) * blocks2pix;
            qreal y = vh/2.0 + (p.z - focusz) * blocks2pix;
            QPointF d = QPointF(x, y);
            QRectF r = icons[D_STRONGHOLD].rect();
            frags.push_back(QPainter::PixmapFragment::create(d, r));

            if (seldo)
            {
                r.moveCenter(d);
                if (r.contains(selx, selz))
                {
                    seltype = D_STRONGHOLD;
                    selpos = p;
                    selvar = 0;
                }
            }
        }
        painter.drawPixmapFragments(frags.data(), frags.size(), icons[D_STRONGHOLD]);
    }

    for (int li = lvb.size()-1; li >= 0; --li)
    {
        Level& l = lvb[li];
        if (li == activelv || li == activelv+1)
            l.update(cachedbiomes, bx0, bz0, bx1, bz1);
        else
            l.update(cachedbiomes, 0, 0, 0, 0);
    }
    for (int sopt = D_DESERT; sopt < D_SPAWN; sopt++)
    {
        Level& l = lvs[sopt];
        if (activelv <= l.viewlv && sshow[sopt] && dim == l.dim)
            l.update(cachedstruct, bx0, bz0, bx1, bz1);
        else if (activelv > l.viewlv+1)
            l.update(cachedstruct, 0, 0, 0, 0);
    }

    // start the spawn and stronghold worker thread if this is the first run
    if (spawn == NULL)
    {
        if (sshow[D_SPAWN] || sshow[D_STRONGHOLD] || (showBB && blocks2pix >= 1.0))
        {
            spawn = (Pos*) -1;
            QThreadPool::globalInstance()->start(new SpawnStronghold(this, wi));
        }
    }

    if (seldo)
    {
        seldo = false;
    }

    if (seltype != D_NONE)
    {
        QPixmap *icon = &icons[seltype];
        if (selvar)
        {
            if (seltype == D_VILLAGE)
                icon = &iconzvil;
        }
        qreal x = vw/2.0 + (selpos.x - focusx) * blocks2pix;
        qreal y = vh/2.0 + (selpos.z - focusz) * blocks2pix;
        QRect iconrec = icon->rect();
        qreal w = iconrec.width() * 1.5;
        qreal h = iconrec.height() * 1.5;
        painter.drawPixmap(x-w/2, y-h/2, w, h, *icon);

        QFont f = QFont();
        f.setBold(true);
        painter.setFont(f);

        QString s = QString::asprintf(" %d,%d", selpos.x, selpos.z);
        int pad = 5;
        QRect textrec = painter.fontMetrics()
                .boundingRect(0, 0, vw, vh, Qt::AlignLeft | Qt::AlignTop, s);

        if (textrec.height() < iconrec.height())
            textrec.setHeight(iconrec.height());

        textrec.translate(pad+iconrec.width(), pad);

        painter.fillRect(textrec.marginsAdded(QMargins(pad+iconrec.width(),pad,pad,pad)),
                         QBrush(QColor(0, 0, 0, 128), Qt::SolidPattern));

        painter.setPen(QPen(QColor(255, 255, 255), 2));
        painter.drawText(textrec, s, QTextOption(Qt::AlignLeft | Qt::AlignVCenter));

        painter.drawPixmap(iconrec.translated(pad,pad), *icon);
    }

    cleancache(cachedbiomes, cachesize);
    cleancache(cachedstruct, cachesize);
}


