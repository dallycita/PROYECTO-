#include "raylib.h"
#include <vector>
#include <cmath>
#include <string>

// pantalla
const int ANCHO = 1024;
const int ALTO  = 640;

// juego
const float FOV  = 0.66f;
const float VEL  = 4.5f;
const float ROT  = 2.0f;
const float SENS = 0.0028f;

// estados
enum Estado { MENU, JUEGO, GANASTE };

// nivel
struct Nivel { int w,h; std::vector<int> cel; };
int id(const Nivel& n, int x, int y){ return y*n.w + x; }
bool solida(const Nivel& n, int x, int y){
    if (x<0||y<0||x>=n.w||y>=n.h) return true;
    int v = n.cel[id(n,x,y)];
    return (v>0 && v!=9); // 9 = salida
}

// color pared
Color colorPared(int t, bool ejeY){
    Color c = {200,200,200,255};
    if (t==1) c = {220, 80, 80,255};
    if (t==2) c = { 80,220, 80,255};
    if (t==3) c = { 80, 80,220,255};
    if (t==4) c = {220,180, 60,255};
    if (ejeY){ c.r*=0.6f; c.g*=0.6f; c.b*=0.6f; }
    return c;
}

// sprite simple de antorcha (4 frames)
struct Antorcha {
    RenderTexture2D rt[4];
    void init(int w=32, int h=48){
        for (int i=0;i<4;i++){
            rt[i] = LoadRenderTexture(w,h);
            BeginTextureMode(rt[i]);
            ClearBackground(BLANK);
            DrawRectangle(w/2-2, h-14, 4, 14, Color{80,60,30,255}); // palo
            float k = 1.0f + 0.06f*i;                               // llama
            DrawCircle(w/2, h-18, 10*k, ORANGE);
            DrawCircle(w/2, h-26,  8*k, YELLOW);
            DrawCircle(w/2, h-34,  5*k, GOLD);
            EndTextureMode();
        }
    }
    void draw(int x, int y, float t){
        int f = (int)(t*8) % 4;
        Rectangle src{0,0,(float)rt[f].texture.width, -(float)rt[f].texture.height};
        Rectangle dst{(float)x,(float)y,(float)rt[f].texture.width,(float)rt[f].texture.height};
        DrawTexturePro(rt[f].texture, src, dst, Vector2{0,0}, 0, WHITE);
    }
    void unload(){ for (int i=0;i<4;i++) UnloadRenderTexture(rt[i]); }
};

int main(){
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(ANCHO, ALTO, "Ray Caster UVG");
    SetTargetFPS(60);

    // audio (acepta .ogg/.mp3/.wav)
    InitAudioDevice();
    Music music{};
    Sound sStep{}, sWin{};
    bool haveMusic=false, haveStep=false, haveWin=false;

    if (FileExists("assets/music.ogg"))      { music = LoadMusicStream("assets/music.ogg"); haveMusic=true; }
    else if (FileExists("assets/music.mp3")) { music = LoadMusicStream("assets/music.mp3"); haveMusic=true; }
    else if (FileExists("assets/music.wav")) { music = LoadMusicStream("assets/music.wav"); haveMusic=true; }
    if (haveMusic){ SetMusicVolume(music, 0.6f); PlayMusicStream(music); }

    if (FileExists("assets/step.wav")) { sStep = LoadSound("assets/step.wav"); haveStep=true; SetSoundVolume(sStep, 0.85f); }
    if (FileExists("assets/win.wav"))  { sWin  = LoadSound("assets/win.wav");  haveWin=true;  SetSoundVolume(sWin, 0.95f); }

    // antorcha
    Antorcha ant; ant.init();

    // mapa (9 = salida)
    Nivel m{16,16,{}};
    int datos[16*16] = {
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,0,0,0,0,0,0,0,0,2,0,0,0,0,0,1,
      1,0,1,1,0,1,1,1,0,2,0,1,1,1,0,1,
      1,0,1,0,0,0,0,1,0,2,0,1,0,0,0,1,
      1,0,1,0,1,1,0,1,0,2,0,1,0,1,0,1,
      1,0,0,0,0,0,0,0,0,2,0,0,0,1,0,1,
      1,0,1,1,1,1,1,1,0,2,1,1,0,1,0,1,
      1,0,1,0,0,0,0,1,0,2,0,1,0,1,0,1,
      1,0,1,0,1,1,0,1,0,2,0,1,0,1,0,1,
      1,0,0,0,0,0,0,0,0,2,0,0,0,0,0,1,
      1,0,1,1,1,1,1,1,1,2,1,1,1,1,0,1,
      1,0,0,0,0,0,0,0,0,2,0,0,0,1,0,1,
      1,0,1,1,1,1,1,1,0,2,1,1,0,1,0,1,
      1,0,0,0,0,0,0,0,0,2,0,0,0,0,0,1,
      1,0,0,0,0,0,0,0,0,0,0,0,0,0,9,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
    };
    m.cel.assign(datos, datos+16*16);

    // jugador
    Vector2 pos = {1.5f, 1.5f};
    float dirX = 1.0f, dirY = 0.0f;
    float plX = 0.0f, plY = FOV;

    Estado estado = MENU;
    bool cursorMostrado = true;
    float stepTimer=0.0f;

    while(!WindowShouldClose()){
        float dt = GetFrameTime();
        if (haveMusic) UpdateMusicStream(music);

        // MENU
        if (estado == MENU){
            if (!cursorMostrado){ ShowCursor(); cursorMostrado = true; }
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("RAY CASTER (UVG)", 60, 70, 50, RAYWHITE);
            DrawText("W/S o flechas: mover   Mouse o <- ->: girar", 60, 140, 24, GRAY);
            DrawText("A/D: strafe   Llega a la casilla amarilla", 60, 170, 24, GRAY);
            DrawText("ENTER para jugar", 60, 220, 28, YELLOW);
            ant.draw(60, 270, GetTime());
            DrawFPS(ANCHO-90,10);
            EndDrawing();
            if (IsKeyPressed(KEY_ENTER)){
                estado = JUEGO;
                HideCursor(); cursorMostrado = false;
                pos = {1.5f, 1.5f};
                dirX = 1.0f; dirY = 0.0f; plX = 0.0f; plY = FOV;
            }
            continue;
        }

        // GANASTE
        if (estado == GANASTE){
            if (!cursorMostrado){ ShowCursor(); cursorMostrado = true; }
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("¡GANASTE!", 60, 80, 60, YELLOW);
            DrawText("ENTER: menu", 60, 160, 28, RAYWHITE);
            ant.draw(60, 210, GetTime());
            EndDrawing();
            if (IsKeyPressed(KEY_ENTER)) estado = MENU;
            continue;
        }

        // JUEGO
        if (cursorMostrado){ HideCursor(); cursorMostrado = false; }

        // girar (mouse)
        Vector2 md = GetMouseDelta();
        float ang = -md.x * SENS;
        float c = cosf(ang), s = sinf(ang);
        float odx=dirX, opx=plX;
        dirX = odx*c - dirY*s;  dirY = odx*s + dirY*c;
        plX  = opx*c - plY*s;   plY  = opx*s + plY*c;

        // girar (flechas)
        if (IsKeyDown(KEY_RIGHT)){
            float a=-ROT*dt, cc=cosf(a), ss=sinf(a);
            float tx=dirX*cc - dirY*ss, ty=dirX*ss + dirY*cc;
            float px=plX*cc  - plY*ss,  py=plX*ss  + plY*cc;
            dirX=tx; dirY=ty; plX=px; plY=py;
        }
        if (IsKeyDown(KEY_LEFT)){
            float a= ROT*dt, cc=cosf(a), ss=sinf(a);
            float tx=dirX*cc - dirY*ss, ty=dirX*ss + dirY*cc;
            float px=plX*cc  - plY*ss,  py=plX*ss  + plY*cc;
            dirX=tx; dirY=ty; plX=px; plY=py;
        }

        // mover (W/S, ↑/↓) y strafe (A/D) + pasos
        bool up   = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
        bool down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
        bool leftStrafe  = IsKeyDown(KEY_A);
        bool rightStrafe = IsKeyDown(KEY_D);

        Vector2 d{0,0};
        if (up)   { d.x += dirX*VEL*dt; d.y += dirY*VEL*dt; }
        if (down) { d.x -= dirX*VEL*dt; d.y -= dirY*VEL*dt; }
        if (leftStrafe)  { d.x += (-dirY)*VEL*dt; d.y += ( dirX)*VEL*dt; }
        if (rightStrafe) { d.x += ( dirY)*VEL*dt; d.y += (-dirX)*VEL*dt; }

        float nx = pos.x + d.x, ny = pos.y + d.y;
        bool moved=false;
        if (!solida(m, (int)nx, (int)pos.y)) { pos.x = nx; moved=true; }
        if (!solida(m, (int)pos.x, (int)ny)) { pos.y = ny; moved=true; }

        if (moved){
            stepTimer += dt;
            if (haveStep && stepTimer > 0.25f){ PlaySound(sStep); stepTimer=0.0f; }
        } else stepTimer = 0.2f;

        // ¿salida?
        if (m.cel[id(m,(int)pos.x,(int)pos.y)] == 9){
            if (haveWin) PlaySound(sWin);
            estado = GANASTE;
            continue;
        }

        // dibujar 3D
        BeginDrawing();
        ClearBackground(BLACK);
        int W=GetScreenWidth(), H=GetScreenHeight();

        DrawRectangle(0,0,W,H/2, Color{60,90,150,255});
        DrawRectangle(0,H/2,W,H/2, Color{50,40,30,255});

        for (int x=0; x<W; x++){
            float camX = 2.0f*x/W - 1.0f;
            float rX = dirX + plX*camX;
            float rY = dirY + plY*camX;

            int mx=(int)pos.x, my=(int)pos.y;
            float dX = (rX==0)?1e30f:fabsf(1.0f/rX);
            float dY = (rY==0)?1e30f:fabsf(1.0f/rY);
            float sX, sY; int stX, stY;
            if (rX<0){ stX=-1; sX=(pos.x-mx)*dX; } else { stX=1; sX=(mx+1.0f-pos.x)*dX; }
            if (rY<0){ stY=-1; sY=(pos.y-my)*dY; } else { stY=1; sY=(my+1.0f-pos.y)*dY; }

            bool hit=false, ejeY=false; int tipo=0;
            while(!hit){
                if (sX < sY){ sX+=dX; mx+=stX; ejeY=false; }
                else        { sY+=dY; my+=stY; ejeY=true;  }
                if (mx<0||my<0||mx>=m.w||my>=m.h){ hit=true; tipo=1; }
                else { int v=m.cel[id(m,mx,my)]; if (v>0 && v!=9){ hit=true; tipo=v; } }
            }

            float dist = (!ejeY)? (sX-dX) : (sY-dY);
            int h = (int)(H/(dist+0.0001f));
            int y0 = -h/2 + H/2, y1 = h/2 + H/2;
            DrawLine(x, y0, x, y1, colorPared(tipo, ejeY));
        }

        // minimapa
        int tam=10, ox=10, oy=10;
        for(int y=0;y<m.h;y++){
            for(int x=0;x<m.w;x++){
                int v = m.cel[id(m,x,y)];
                Color c = (v==0)? Color{25,25,30,255} :
                          (v==9)? Color{240,240,120,255} : Color{120,120,120,255};
                DrawRectangle(ox+x*tam, oy+y*tam, tam, tam, c);
            }
        }
        DrawCircle(ox+(int)(pos.x*tam), oy+(int)(pos.y*tam), 3, RED);
        DrawLine(ox+(int)(pos.x*tam), oy+(int)(pos.y*tam),
                 ox+(int)((pos.x+dirX*0.8f)*tam), oy+(int)((pos.y+dirY*0.8f)*tam), RED);

        ant.draw(ANCHO-80, 20, GetTime()); // antorcha en HUD

        DrawFPS(W-90,10);
        EndDrawing();
    }

    ant.unload();
    if (haveMusic){ StopMusicStream(music); UnloadMusicStream(music); }
    if (haveStep) UnloadSound(sStep);
    if (haveWin)  UnloadSound(sWin);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
