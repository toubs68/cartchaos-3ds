// ============================================================================
//  CART CHAOS 3D - a downhill shopping-cart physics game for the Nintendo 3DS
//  (homebrew / Luma3DS). Single-file source -> builds to a .3dsx with devkitARM.
//
//  BUILD (on a PC with devkitARM):   make
//  Then copy the generated .3dsx + .smdh to your SD card's /3ds/ folder and
//  launch via the Homebrew Launcher. Requires a homebrewed ("Au" region is
//  fine) console - stock 3DS cannot run homebrew.
//
//  CONTROLS
//   A        : sprint / hop into the cart / restart
//   D-Pad    : lean & steer (Up/Down = lean back/forward, Left/Right = steer)
//   CirclePad: fine steer
//   B        : drink a stored beverage for energy
//   START    : back to menu / restart after a crash
//
//  The pure simulation (World/Game) is the same code that was headless-tested
//  in source/sim and verified on-device-safe (no NaNs, capped speeds, always a
//  clear lane). Rendering uses citro2d; audio is synthesized with ndsp.
// ============================================================================

#include <3ds.h>
#include <citro2d.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>

// ----------------------------------------------------------------------------
//  Small math helpers
// ----------------------------------------------------------------------------
static const float PI = 3.14159265f;
static inline float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float lerpf(float a, float b, float t){ return a+(b-a)*t; }
static inline float approachf(float cur, float target, float rate, float dt){
    float k = 1.0f - std::exp(-rate*dt);
    return cur + (target-cur)*k;
}
struct Rng { uint32_t s; explicit Rng(uint32_t seed=0x9e3779b9u):s(seed?seed:1u){}
    uint32_t next(){ uint32_t x=s; x^=x<<13; x^=x>>17; x^=x<<5; s=x; return x; }
    float f(){ return (next()&0x00ffffffu)/16777216.0f; }
    float range(float a,float b){ return a+(b-a)*f(); }
    int ri(int a,int b){ return a+(int)(f()*(b-a)); }
};

// ----------------------------------------------------------------------------
//  World generation
// ----------------------------------------------------------------------------
enum class Biome { CITY, INDUSTRIAL, COUNTRY, CLIFF };
enum class ObstacleKind { ROCK, CONE, POLE, BARRIER, TREE, BUILDING_EDGE, GUARDRAIL };

struct Obstacle { float x; float lateral; float halfW; ObstacleKind kind; bool onGround; };
struct Ramp { float x; float length; float rise; };
struct Pickup { float x; float lateral; float y; bool taken; int energy; };
struct Car { float x; float lateral; float speed; float halfLen; };

struct World {
    float totalLength = 6000.0f;
    float startSprint = 120.0f;
    static const int GRID_N = 4096;
    static const float GRID_DX;
    float height[GRID_N];

    Biome biomeAt(float x) const {
        float t = x/totalLength;
        if (t<0.30f) return Biome::CITY;
        if (t<0.55f) return Biome::INDUSTRIAL;
        if (t<0.80f) return Biome::COUNTRY;
        return Biome::CLIFF;
    }
    float roadHeight(float x) const {
        if (x<0) x=0;
        int i=(int)(x/GRID_DX);
        if (i>=GRID_N-1) i=GRID_N-2;
        float f=(x-i*GRID_DX)/GRID_DX;
        return lerpf(height[i],height[i+1],f);
    }
    float roadGrade(float x) const {
        float d=0.75f; return (roadHeight(x+d)-roadHeight(x-d))/(2.0f*d);
    }
    std::vector<Obstacle> obstacles;
    std::vector<Ramp> ramps;
    std::vector<Pickup> pickups;
    std::vector<Car> cars;
    void generate(uint32_t seed);
};
const float World::GRID_DX = 1.5f;

void World::generate(uint32_t seed){
    Rng rng(seed);
    std::memset(height,0,sizeof(height));
    float h=0.0f;
    int startIdx=(int)(startSprint/GRID_DX);
    float slope=0.0f;
    for (int i=0;i<GRID_N;++i){
        if (i<startIdx){ slope=approachf(slope,-0.01f,1.0f,0.02f); }
        else {
            float tlen=(i-startIdx)*GRID_DX;
            float speedup=0.02f*(tlen/(totalLength-startSprint));
            float targetSlope=-0.085f-speedup;
            slope+=rng.range(-0.006f,0.006f);
            slope=approachf(slope,targetSlope,0.6f,1.0f);
            slope=clampf(slope,-0.28f,0.06f);
        }
        h+=slope*GRID_DX; height[i]=h;
    }
    float base=height[startIdx];
    for (int i=0;i<GRID_N;++i) height[i]-=base;

    obstacles.clear();
    const float lanes[3]={-0.7f,0.0f,0.7f};
    const float obsStep=34.0f;
    int prevLane=-1;
    for (float x=startSprint+30.0f; x<totalLength-40.0f; x+=obsStep){
        int lane=rng.ri(0,3);
        if (lane==prevLane && rng.f()<0.6f) lane=(lane+1)%3;
        prevLane=lane;
        Obstacle o; o.x=x+rng.range(-5.0f,5.0f); o.lateral=lanes[lane];
        o.halfW=rng.range(0.18f,0.26f);
        o.kind=(ObstacleKind)rng.ri(0,(int)ObstacleKind::GUARDRAIL+1);
        o.onGround=true; obstacles.push_back(o);
    }
    ramps.clear();
    for (float x=startSprint+60.0f; x<totalLength-100.0f; x+=rng.range(120.0f,220.0f)){
        Ramp r; r.x=x; r.length=rng.range(8.0f,16.0f); r.rise=rng.range(1.2f,3.0f); ramps.push_back(r);
    }
    pickups.clear();
    for (float x=startSprint+20.0f; x<totalLength-30.0f; x+=rng.range(40.0f,75.0f)){
        Pickup p; p.x=x; p.lateral=rng.range(-0.8f,0.8f); p.y=rng.range(0.4f,1.1f);
        p.taken=false; p.energy=rng.ri(18,34); pickups.push_back(p);
    }
    cars.clear();
    const float carLanes[3]={-0.75f,0.0f,0.75f};
    const float carStep=42.0f;
    for (float x=startSprint+90.0f; x<totalLength-30.0f; x+=carStep){
        Car c; c.x=x+rng.range(-6.0f,6.0f); c.lateral=carLanes[rng.ri(0,3)];
        bool oncoming = rng.f()<0.5f;
        c.speed=(oncoming?-1.0f:1.0f)*rng.range(5.0f,9.0f);
        c.halfLen=rng.range(1.6f,2.2f); cars.push_back(c);
    }
}

// ----------------------------------------------------------------------------
//  Game simulation
// ----------------------------------------------------------------------------
enum class State { MENU, SPRINT, RIDE, AIR, CRASH, WIN, LOSE };
struct Input { bool push=false; bool jump=false; float lean=0; float steer=0; bool useDrink=false; bool start=false; bool confirm=false; };
struct Cart { float x=0,y=0,vy=0,vx=0,lateral=0,latVel=0,pitch=0,roll=0,wheelSpin=0,rattle=0; };
struct EventLog { bool crashed=false,launched=false,screamed=false,drank=false; int drinksCollected=0,carsPassed=0; void clear(){crashed=launched=screamed=drank=false;} };

class Game {
public:
    Game(uint32_t seed=12345){ reset(seed); }
    void reset(uint32_t seed){
        seed_=seed; world_.generate(seed);
        cart_=Cart{}; state_=State::MENU; energy_=100; drinks_=0; topSpeed_=0;
        runTime_=0; crashTimer=0; shake=0; prev_=Input{}; boulderActive_=false; boulderX_=0; ev_=EventLog{};
    }
    void step(float dt, const Input& inp){
        ev_.clear();
        switch(state_){
            case State::MENU: if(inp.confirm||inp.push){state_=State::SPRINT;cart_.x=0;cart_.vx=0;} break;
            case State::SPRINT: updateSprint(dt,inp); break;
            case State::RIDE: updateRide(dt,inp); break;
            case State::AIR: updateAir(dt,inp); break;
            case State::CRASH: crashTimer+=dt; shake=approachf(shake,0,2.5f,dt);
                if((inp.confirm||inp.start)&&crashTimer>0.8f) reset(seed_); break;
            case State::WIN: case State::LOSE: shake=approachf(shake,0,2.5f,dt);
                if((inp.confirm||inp.start)&&crashTimer>0.8f) reset(seed_); break;
        }
        prev_=inp;
    }
    State state() const { return state_; }
    const World& world() const { return world_; }
    const Cart& cart() const { return cart_; }
    int energy() const { return energy_; }
    int drinks() const { return drinks_; }
    float topSpeed() const { return topSpeed_; }
    const EventLog& events() const { return ev_; }
    float finishDistance() const { return world_.totalLength-cart_.x; }
    float boulderDistance() const { return boulderX_-cart_.x; }
    float crashTimer=0, shake=0; const char* crashReason="";
private:
    static inline float leanToAccel(float lean){ return lean*3.0f; }
    void spawnBoulder(){ boulderActive_=true; boulderX_=cart_.x-90.0f; }
    void doCrash(const char* r){ state_=State::CRASH; ev_.crashed=true; crashTimer=0; shake=1.0f; crashReason=r; }
    void land(){
        float ang=std::fabs(cart_.pitch);
        if (ang>0.9f || (ang>0.6f&&cart_.vx>30.0f)){ doCrash("bad landing"); return; }
        cart_.y=0; cart_.vy=0;
        cart_.pitch=approachf(cart_.pitch,world_.roadGrade(cart_.x),8.0f,0.016f);
        state_=State::RIDE;
    }
    void updateSprint(float dt,const Input& inp){
        if (inp.push) cart_.vx+=7.0f*dt;
        cart_.vx=clampf(cart_.vx,0.0f,9.0f*1.4f);
        cart_.x+=cart_.vx*dt;
        cart_.wheelSpin+=cart_.vx*dt*1.2f;
        cart_.rattle=clampf(cart_.vx/12.0f,0,1);
        cart_.pitch=world_.roadGrade(cart_.x);
        if (cart_.x>=world_.startSprint || inp.jump){
            if (cart_.vx>2.0f){ state_=State::RIDE; spawnBoulder(); }
        }
    }
    void updateRide(float dt,const Input& inp){
        runTime_+=dt;
        World& w=world_;
        float grade=w.roadGrade(cart_.x);
        float g=9.81f;
        float a=-g*grade + leanToAccel(inp.lean);
        a-=0.9f*cart_.vx*0.02f; a-=0.6f;
        if (inp.push && energy_>0){ a+=4.2f; energy_-=(int)(18.0f*dt); if(energy_<0)energy_=0; }
        cart_.vx+=a*dt; if(cart_.vx<0)cart_.vx=0; if(cart_.vx>40.0f)cart_.vx=40.0f;

        // lateral position controller
        float targetLat=clampf(cart_.lateral+inp.steer*0.9f,-0.95f,0.95f);
        float err=targetLat-cart_.lateral;
        float maxLat=7.5f;
        float desiredVel=clampf(err*6.0f,-maxLat,maxLat);
        cart_.latVel=approachf(cart_.latVel,desiredVel,14.0f,dt);
        cart_.lateral+=cart_.latVel*dt;
        cart_.lateral=clampf(cart_.lateral,-1.02f,1.02f);
        cart_.roll=approachf(cart_.roll,-inp.steer*0.4f+inp.lean*0.2f,8.0f,dt);

        cart_.x+=cart_.vx*dt;
        cart_.wheelSpin+=cart_.vx*dt*1.2f;
        cart_.rattle=clampf(cart_.vx/14.0f,0,1);
        cart_.pitch=approachf(cart_.pitch,grade,6.0f,dt);
        cart_.y=0;

        for (auto& r:w.ramps){
            if (cart_.x>r.x && cart_.x<r.x+r.length && cart_.vx>5.0f){
                cart_.vy=(cart_.vx*0.45f)*(r.rise/1.8f);
                cart_.y=0.05f; state_=State::AIR; ev_.launched=true; shake=0.4f; break;
            }
        }
        for (auto& p:w.pickups){
            if (!p.taken && std::fabs(p.x-cart_.x)<1.2f && std::fabs(p.lateral-cart_.lateral)<0.7f &&
                std::fabs(p.y-(cart_.y+0.8f))<1.4f){
                p.taken=true; drinks_++; energy_=clampf(energy_+p.energy,0,120); ev_.drank=true; ev_.drinksCollected++;
            }
        }
        if (inp.useDrink && drinks_>0 && energy_<100){ drinks_--; energy_=clampf(energy_+30,0,120); ev_.drank=true; }

        for (auto& o:w.obstacles){
            if (std::fabs(o.x-cart_.x)<(o.halfW+0.5f) && std::fabs(o.lateral-cart_.lateral)<(o.halfW+0.4f)){
                if (cart_.y<1.4f){ doCrash("obstacle"); return; }
            }
        }
        for (auto& c:w.cars){
            c.x+=c.speed*dt;
            if (c.x>cart_.x+400) c.x=cart_.x-400+((int)(c.speed)%80);
            if (c.x<cart_.x-600) c.x=cart_.x+200;
            float dx=c.x-cart_.x;
            if (std::fabs(dx)<(c.halfLen+0.7f) && std::fabs(c.lateral-cart_.lateral)<0.34f){
                if (cart_.y<1.2f){ doCrash("car"); return; }
            }
            if (cart_.y<1.2f && std::fabs(dx)<3.5f && std::fabs(c.lateral-cart_.lateral)<1.0f) ev_.screamed=true;
            if (std::fabs(dx)<4.0f) ev_.carsPassed++;
        }
        if (boulderActive_){
            float bg=w.roadGrade(cart_.x);
            boulderX_+=(6.0f+cart_.vx*0.25f)*dt+2.5f*(-bg)*dt;
            if (boulderX_>=cart_.x-0.5f){ doCrash("boulder"); return; }
        }
        if (std::fabs(cart_.lateral)>1.02f){ doCrash("off road"); return; }
        if (cart_.vx>topSpeed_) topSpeed_=cart_.vx;
        if (cart_.x>=w.totalLength) state_=State::WIN;
    }
    void updateAir(float dt,const Input& inp){
        runTime_+=dt;
        World& w=world_;
        cart_.vy-=9.81f*dt; cart_.x+=cart_.vx*dt; cart_.y+=cart_.vy*dt;
        cart_.wheelSpin+=cart_.vx*dt*1.2f;
        cart_.pitch+=inp.lean*1.5f*dt;
        cart_.roll=approachf(cart_.roll,-inp.steer*0.6f,6.0f,dt);
        cart_.lateral+=inp.steer*1.2f*dt;
        cart_.lateral=clampf(cart_.lateral,-1.05f,1.05f);
        float gh=w.roadHeight(cart_.x); (void)gh;
        if (cart_.y<=0 && cart_.vy<0) land();
        for (auto& c:w.cars) c.x+=c.speed*dt;
        if (cart_.vx>topSpeed_) topSpeed_=cart_.vx;
        if (cart_.x>=w.totalLength) state_=State::WIN;
    }
    World world_; Cart cart_; State state_=State::MENU; Input prev_;
    uint32_t seed_=0; int energy_=100; int drinks_=0; float topSpeed_=0;
    float boulderX_=0; bool boulderActive_=false; float runTime_=0; EventLog ev_;
};

// ----------------------------------------------------------------------------
//  3DS rendering + input + audio
// ----------------------------------------------------------------------------
#define SCREEN_W 400
#define SCREEN_H 240

// Palette (light cartoon style). In citro2d colors are plain u32 (C2D_Color32).
static const u32 C_SKY_CITY    = C2D_Color32(150,200,235,255);
static const u32 C_SKY_IND     = C2D_Color32(180,175,160,255);
static const u32 C_SKY_COUNTRY = C2D_Color32(170,215,180,255);
static const u32 C_SKY_CLIFF   = C2D_Color32(200,200,235,255);
static const u32 C_ROAD        = C2D_Color32(70,70,78,255);
static const u32 C_ROAD_LINE   = C2D_Color32(230,220,90,255);
static const u32 C_GRASS       = C2D_Color32(110,180,90,255);
static const u32 C_CART        = C2D_Color32(210,90,60,255);
static const u32 C_CART_DARK   = C2D_Color32(150,50,30,255);
static const u32 C_WHEEL       = C2D_Color32(40,40,45,255);

// Forward declaration for the text helper (defined near audio section).
static void txt(float x, float y, float s, u32 col, const char* fmt, ...);

// ndsp tone channels
static ndspWaveBuf waveBuffers[2];
static int16_t toneBuf[2][256];
static int curTone=0;

// Screen-space projection for the road. We draw the road as a series of
// quads built from the world height profile, with the camera behind/above the
// cart looking downhill.
struct Cam {
    float x;        // camera tracks cart.x
    float camY;     // height offset
};

static u32 skyColorFor(Biome b){
    switch(b){ case Biome::INDUSTRIAL: return C_SKY_IND;
        case Biome::COUNTRY: return C_SKY_COUNTRY;
        case Biome::CLIFF: return C_SKY_CLIFF; default: return C_SKY_CITY; }
}

// Draw a filled triangle helper
static void fillTri(u32 c, float x0,float y0,float x1,float y1,float x2,float y2){
    C2D_DrawTriangle(x0,y0,c,x1,y1,c,x2,y2,c,0);
}
static void fillQuad(u32 c, float x0,float y0,float x1,float y1,float x2,float y2,float x3,float y3){
    C2D_DrawTriangle(x0,y0,c,x1,y1,c,x2,y2,c,0);
    C2D_DrawTriangle(x2,y2,c,x1,y1,c,x3,y3,c,0);
}

// Convert a world (dist, lateralOffset, height) to screen coords given camera.
static float projectY(float worldX, float lateral, float height, const Game& g, float* outX){
    float camX = g.cart().x;
    float dyRoad = (g.world().roadHeight(worldX) - g.cart().y);
    // pseudo-3D: things ahead (worldX>camX) go up the screen; perspective scale
    float ahead = worldX - camX;                 // meters ahead (can be negative)
    float scale = 120.0f / (120.0f + ahead*2.2f);
    if (scale<0.05f) scale=0.05f;
    float horizon = SCREEN_H*0.30f;
    float screenY = horizon + (20.0f - (g.world().roadHeight(camX) - height)) * scale - ahead*scale*0.9f;
    screenY += (g.world().roadHeight(camX) - g.cart().y) * scale; // follow cart vertically
    float latScreen = lateral * 150.0f * scale;
    *outX = SCREEN_W*0.5f + latScreen;
    return screenY;
}

static void drawWorld(C3D_RenderTarget* top, Game& g){
    C2D_SceneBegin(top);
    C2D_TargetClear(top, skyColorFor(g.world().biomeAt(g.cart().x)));

    float shake = g.shake;
    float ox = shake>0 ? ((rand()%100)/100.0f-0.5f)*shake*10.0f : 0;
    float oy = shake>0 ? ((rand()%100)/100.0f-0.5f)*shake*10.0f : 0;
    C2D_Prepare();

    // --- distant parallax skyline (per biome) ---
    Biome b = g.world().biomeAt(g.cart().x);
    if (b==Biome::CITY || b==Biome::INDUSTRIAL){
        for (int i=0;i<14;i++){
            float bx = ((i*47.0f - (g.cart().x*0.15f)) ) ; bx = fmodf(bx, SCREEN_W+80); if(bx<0)bx+=SCREEN_W+80;
            int bh = 30 + ((i*53)%70);
            C2D_DrawRectSolid(bx, SCREEN_H*0.30f-bh, 0, 26, bh, C2D_Color32(90,100,120,255));
        }
    } else if (b==Biome::COUNTRY){
        for (int i=0;i<10;i++){
            float bx = fmodf(i*60.0f - g.cart().x*0.1f, SCREEN_W+100); if(bx<0)bx+=SCREEN_W+100;
            C2D_DrawTriangle(bx,SCREEN_H*0.30f,C2D_Color32(70,120,70,255),
                             bx+40,SCREEN_H*0.30f,C2D_Color32(70,120,70,255),
                             bx+20,SCREEN_H*0.30f-40,C2D_Color32(60,110,60,255),0);
        }
    } else {
        for (int i=0;i<8;i++){
            float bx = fmodf(i*70.0f - g.cart().x*0.08f, SCREEN_W+120); if(bx<0)bx+=SCREEN_W+120;
            C2D_DrawTriangle(bx,SCREEN_H*0.30f,C2D_Color32(120,110,140,255),
                             bx+50,SCREEN_H*0.30f,C2D_Color32(120,110,140,255),
                             bx+25,SCREEN_H*0.30f-90,C2D_Color32(110,100,130,255),0);
        }
    }

    // --- road surface as strips ahead of the cart ---
    const int SEG = 26;
    float step = 9.0f;
    for (int i=0;i<SEG;i++){
        float x0 = g.cart().x + i*step - 6.0f;
        float x1 = x0 + step;
        if (x1 > g.world().totalLength) break;
        // left/right road edges in lateral space (-1..1)
        float lx0,ly0,lx1,ly1,rx0,ry0,rx1,ry1;
        float h0 = g.world().roadHeight(x0), h1=g.world().roadHeight(x1);
        ly0=projectY(x0,-1.0f,h0,g,&lx0);
        ry0=projectY(x0, 1.0f,h0,g,&rx0);
        ly1=projectY(x1,-1.0f,h1,g,&lx1);
        ry1=projectY(x1, 1.0f,h1,g,&rx1);
        lx0+=ox; ly0+=oy; rx0+=ox; ry0+=oy; lx1+=ox; ly1+=oy; rx1+=ox; ry1+=oy;
        fillQuad(C_ROAD, lx0,ly0, rx0,ry0, rx1,ry1, lx1,ly1);
        // center dashed line
        if (((int)(x0/6))%2==0){
            float c0x,c0y,c1x,c1y;
            c0y=projectY(x0,0,h0,g,&c0x); c1y=projectY(x1,0,h1,g,&c1x);
            c0x+=ox;c0y+=oy;c1x+=ox;c1y+=oy;
            fillQuad(C_ROAD_LINE, c0x-2,c0y-1, c0x+2,c0y-1, c1x+2,c1y-1, c1x-2,c1y-1);
        }
    }

    // --- obstacles, pickups, cars drawn as billboards ---
    auto drawBill = [&](float wx, float lat, float baseY, u32 col, float w, float hh){
        float h = g.world().roadHeight(wx);
        float sx,sy; sy=projectY(wx,lat,h+baseY,g,&sx); sx+=ox; sy+=oy;
        float scale = 120.0f/(120.0f+(wx-g.cart().x)*2.2f); if(scale<0.05f)scale=0.05f;
        float W=w*scale*150.0f, H=hh*scale*150.0f;
        C2D_DrawRectSolid(sx-W, sy-H, 0, W*2, H, col);
    };
    for (auto& o:g.world().obstacles){
        if (o.x < g.cart().x-10 || o.x > g.cart().x+200) continue;
        u32 c = C2D_Color32(120,120,130,255);
        switch(o.kind){
            case ObstacleKind::ROCK: c=C2D_Color32(120,110,100,255);break;
            case ObstacleKind::CONE: c=C2D_Color32(230,120,40,255);break;
            case ObstacleKind::POLE: c=C2D_Color32(60,60,70,255);break;
            case ObstacleKind::BARRIER: c=C2D_Color32(220,60,60,255);break;
            case ObstacleKind::TREE: c=C2D_Color32(40,120,50,255);break;
            case ObstacleKind::BUILDING_EDGE: c=C2D_Color32(150,140,150,255);break;
            case ObstacleKind::GUARDRAIL: c=C2D_Color32(200,200,210,255);break;
        }
        drawBill(o.x,o.lateral,0.0f,c,0.18f,0.7f);
    }
    for (auto& p:g.world().pickups){
        if (p.taken) continue;
        if (p.x < g.cart().x-10 || p.x > g.cart().x+200) continue;
        drawBill(p.x,p.lateral,p.y,C2D_Color32(60,180,250,255),0.12f,0.5f);
    }
    for (auto& c:g.world().cars){
        if (c.x < g.cart().x-60 || c.x > g.cart().x+250) continue;
        drawBill(c.x,c.lateral,0.3f, c.speed<0?C2D_Color32(230,80,80,255):C2D_Color32(90,140,230,255),0.35f,0.9f);
    }

    // --- boulder chasing ---
    if (g.boulderDistance() < 250){
        drawBill(g.boulderDistance()+g.cart().x, 0.0f, 0.0f, C2D_Color32(90,80,70,255), 0.5f, 1.2f);
    }

    // --- the cart (rider + metal basket), a billboard near horizon ---
    {
        float sx,sy; sy=projectY(g.cart().x, g.cart().lateral, g.cart().y, g, &sx);
        sx+=ox; sy+=oy;
        float scale = 1.0f;
        float bw=36, bh=26;
        // shadow
        C2D_DrawRectSolid(sx-bw*0.6, sy-2, 0, bw*1.2, 5, C2D_Color32(0,0,0,90));
        // wheels
        C2D_DrawRectSolid(sx-bw*0.5-3, sy-4-3, 0, 6, 6, C_WHEEL);
        C2D_DrawRectSolid(sx+bw*0.5-3, sy-4-3, 0, 6, 6, C_WHEEL);
        // basket body (tilts with pitch/roll)
        float tilt = g.cart().roll*0.5f;
        fillQuad(C_CART, sx-bw*0.5, sy-bh, sx+bw*0.5, sy-bh-(tilt*10), sx+bw*0.5, sy-4-(tilt*6), sx-bw*0.5, sy-4);
        // rider head
        C2D_DrawRectSolid(sx-4, sy-bh-8-4, 0, 8, 8, C2D_Color32(245,205,160,255));
        // speed lines
        if (g.cart().vx>14){
            for (int i=0;i<6;i++){
                float lx = sx + ((i*53)%120)-60;
                float ly = sy - 10 - i*6;
                C2D_DrawRectSolid(lx, ly, 0, 18+g.cart().vx, 2, C2D_Color32(255,255,255,120));
            }
        }
    }

    // --- speed effect vignette ---
    if (g.cart().vx>20){
        float a = clampf((g.cart().vx-20)/20.0f,0,1)*120;
        C2D_DrawRectSolid(0,0,0,SCREEN_W,8,C2D_Color32(255,255,255,(u8)a));
    }
}

static void drawHUD(C3D_RenderTarget* bot, Game& g){
    C2D_SceneBegin(bot);
    C2D_TargetClear(bot, C2D_Color32(20,24,30,255));
    C2D_Prepare();
    txt(10,10,0.6f,C2D_Color32(255,255,255,255),"SPEED %3.0f km/h", g.cart().vx*3.6f);
    txt(10,30,0.5f,C2D_Color32(255,255,255,255),"ENERGY");
    // energy bar
    C2D_DrawRectSolid(70,30,0,100,8,C2D_Color32(60,60,60,255));
    int e=g.energy(); C2D_DrawRectSolid(70,30,0,100*clampf(e/100.0f,0,1),8,
        e>40?C2D_Color32(90,220,90,255):C2D_Color32(230,90,60,255));
    txt(10,48,0.5f,C2D_Color32(120,200,255,255),"DRINKS %d", g.drinks());
    txt(10,68,0.5f,C2D_Color32(255,230,120,255),"DIST %4.0f m", g.finishDistance());
    float bd=g.boulderDistance();
    if (bd<200){ txt(10,90,0.55f,C2D_Color32(255,120,60,255),"BOULDER %3.0f m!", bd); }
}

static void drawOverlay(C3D_RenderTarget* top, Game& g){
    C2D_SceneBegin(top); C2D_Prepare();
    if (g.state()==State::MENU){
        C2D_DrawRectSolid(0,0,0,SCREEN_W,SCREEN_H,C2D_Color32(0,0,0,150));
        txt(40,70,1.1f,C2D_Color32(255,200,80,255),"CART CHAOS 3D");
        txt(60,110,0.6f,C2D_Color32(255,255,255,255),"A: SPRINT INTO THE CART");
        txt(60,135,0.6f,C2D_Color32(255,255,255,255),"D-PAD: LEAN / STEER");
        txt(60,160,0.6f,C2D_Color32(255,255,255,255),"B: DRINK FOR ENERGY");
        txt(60,190,0.6f,C2D_Color32(255,255,255,255),"OUTRUN THE BOULDER!");
    } else if (g.state()==State::CRASH){
        C2D_DrawRectSolid(0,0,0,SCREEN_W,SCREEN_H,C2D_Color32(120,0,0,140));
        txt(90,90,1.0f,C2D_Color32(255,80,80,255),"CRASHED!");
        txt(70,130,0.6f,C2D_Color32(255,255,255,255),"%s", g.crashReason);
        txt(80,170,0.6f,C2D_Color32(255,255,255,255),"PRESS A / START");
    } else if (g.state()==State::WIN){
        C2D_DrawRectSolid(0,0,0,SCREEN_W,SCREEN_H,C2D_Color32(0,80,0,160));
        txt(80,90,1.1f,C2D_Color32(120,255,120,255),"YOU MADE IT!");
        txt(80,150,0.6f,C2D_Color32(255,255,255,255),"PRESS A TO RIDE AGAIN");
    }
}

// --- ndsp audio: simple square-wave tones for sfx ---
static void toneInit(){
    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetRate(0, 22050.0f);
    for (int b=0;b<2;b++){ std::memset(toneBuf[b],0,sizeof(toneBuf[b]));
        waveBuffers[b].data_vaddr=toneBuf[b];
        waveBuffers[b].nsamples=256;
        waveBuffers[b].status=NDSP_WBUF_FREE;
    }
}
static void playTone(float freq, float dur, float vol){
    int b=curTone; curTone^=1;
    int n=256; float tv=1.0f/(float)n;
    for (int i=0;i<n;i++){
        float ph = fmodf((float)i*(freq/22050.0f),1.0f);
        toneBuf[b][i]=(int16_t)( (ph<0.5f? 3000:-3000) * vol );
    }
    ndspChnWaveBufAdd(0,&waveBuffers[b]);
    (void)tv;(void)dur;
}
static void sfx(int id){
    switch(id){
        case 0: playTone(180.0f,0.1f,0.4f); break;   // roll hum (throttled elsewhere)
        case 1: playTone(80.0f,0.5f,0.9f); break;    // crash
        case 2: playTone(520.0f,0.15f,0.5f); break;  // drink
        case 3: playTone(900.0f,0.25f,0.6f); break;  // scream
        case 4: playTone(300.0f,0.08f,0.3f); break;  // launch
    }
}

// --- text via system font (no external assets needed) ---
static C2D_Font g_font = NULL;
static C2D_TextBuf g_textBuf = NULL;
#include <cstdarg>
static void txt(float x, float y, float s, u32 col, const char* fmt, ...){
    char buf[160]; va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof buf,fmt,ap); va_end(ap);
    C2D_Text t; C2D_TextParse(&t, g_textBuf, buf);
    C2D_DrawText(&t, C2D_AlignLeft, x, y, 0.5f, s, s, col);
}

// ----------------------------------------------------------------------------
//  Main
// ----------------------------------------------------------------------------
int main(){
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    toneInit();
    g_font = C2D_FontLoadSystem(CFG_REGION_USA);
    if (!g_textBuf) { g_textBuf = C2D_TextBufNew(4096); C2D_TextBufSetFont(g_textBuf, g_font); }

    Game g(0x1234 + (uint32_t)osGetTime());
    Input inp{}; float rollAccum=0;
    bool prevA=false, prevB=false, prevStart=false;

    while (aptMainLoop()){
        hidScanInput();
        u32 k = hidKeysHeld();

        bool a = (k & KEY_A)!=0;
        bool b = (k & KEY_B)!=0;
        bool start = (k & KEY_START)!=0;
        bool up=(k&KEY_DUP)!=0, down=(k&KEY_DDOWN)!=0, left=(k&KEY_DLEFT)!=0, right=(k&KEY_DRIGHT)!=0;
        float cp = 0; // circle pad
        circlePosition cpad; hidCircleRead(&cpad);
        if (cpad.dx < -20) cp=-1; else if (cpad.dx>20) cp=1;

        inp.push = a || up;                 // A or Up = sprint / pedal push
        inp.jump = a && !prevA;             // edge: hop in
        inp.lean = (down?-1.0f:0.0f) + (up?0.6f:0.0f); // up=lean fwd(accel), down=brake
        if (up) inp.lean=0.7f; if (down) inp.lean=-0.7f;
        inp.steer = (left?-1.0f:0.0f)+(right?1.0f:0.0f); if (cp!=0) inp.steer=cp;
        inp.useDrink = b && !prevB;
        inp.start = start && !prevStart;
        inp.confirm = a && !prevA;

        g.step(1.0f/60.0f, inp);

        // audio cues from events
        if (g.events().crashed) sfx(1);
        else if (g.events().launched) sfx(4);
        else if (g.events().drank) sfx(2);
        else if (g.events().screamed) { static float t=0; if((t+=1/60.f)>0.4f){t=0;sfx(3);} }
        rollAccum += g.cart().rattle;
        if (g.state()==State::RIDE && g.cart().rattle>0.3f && ((int)(rollAccum)%(int)(12-g.cart().rattle*8))==0) sfx(0);

        prevA=a; prevB=b; prevStart=start;

        drawWorld(top, g);
        drawHUD(bot, g);
        drawOverlay(top, g);

        C3D_FrameEnd(0);
        gspWaitForVBlank();
    }

    ndspExit();
    if (g_font) C2D_FontFree(g_font);
    if (g_textBuf) C2D_TextBufDelete(g_textBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
