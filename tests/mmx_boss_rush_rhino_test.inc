static void rush_rhino_save_ppm(const char *path,const uint32_t *pixels) {
  FILE *f=fopen(path,"wb");check(f!=NULL,"Rhino capture opens");
  fprintf(f,"P6\n256 224\n255\n");
  for(unsigned i=0;i<256*224;++i) {
    uint8_t rgb[3]={(uint8_t)(pixels[i]>>16),(uint8_t)(pixels[i]>>8),(uint8_t)pixels[i]};
    fwrite(rgb,1,3,f);
  }
  check(!fclose(f),"Rhino capture writes");
}
/* Count pixels around an object that use the boss's own source palette. */
static unsigned rush_rhino_art_pixels(const uint32_t *pixels,unsigned object,unsigned id) {
  const uint16_t *colors=MmxBossRushForeignColors(id);
  if(!colors) return 0;
  int cx=(int)eagle_word(g_ram+object+5)-(int)eagle_word(g_ram+0x1e4d);
  int cy=(int)eagle_word(g_ram+object+8)-(int)eagle_word(g_ram+0x1e50);
  unsigned hits=0;
  for(int y=cy-48;y<cy+24;++y) for(int x=cx-48;x<cx+48;++x) {
    if(x<0 || x>=256 || y<0 || y>=224) continue;
    uint32_t p=pixels[y*256+x];
    for(unsigned c=1;c<16;++c) {
      unsigned r=(colors[c]&31)*255/31,g=((colors[c]>>5)&31)*255/31,b=((colors[c]>>10)&31)*255/31;
      int dr=(int)((p>>16)&255)-(int)r,dg=(int)((p>>8)&255)-(int)g,db=(int)(p&255)-(int)b;
      if(dr*dr+dg*dg+db*db<=48) {++hits;break;}
    }
  }
  return hits;
}
/* Fire one native shot at an idle Rhino and report his first damage, whether
 * his relocated weakness test made him flinch, and his profile row then.
 * page 0 fires X1 weapon `weapon` (0 = buster); pages 1/2 fire that loaded
 * source pack's weapon through its own selection, as a player would.
 * profile: force a row before firing (0 = drill-charge deflect), or -1. */
typedef struct RhinoHit {unsigned damage;bool flinched;unsigned profile;} RhinoHit;
static RhinoHit rush_rhino_shoot(uint8_t *snapshot,size_t size,unsigned object,unsigned page,
                                 unsigned weapon,int profile) {
  check(RtlLoadSnapshotFromMemory(snapshot,size),"Rhino weakness case restores combat");
  MmxBossRushState s=MmxBossRushGetState();
  unsigned penguin=s.bosses[0].object;RhinoHit result={0,false,0};
  unsigned start_hp=g_ram[object+0x27]&127;bool hit=false;
  MmxWeaponsState w=MmxWeaponsGetState();
  w.initialized=1;w.page=(uint8_t)page;w.weapon=(uint8_t)(page?weapon:0);
  if(page) w.energy[(page-1)*8+weapon-1]=28;
  MmxWeaponsSetState(w);
  for(unsigned t=0;t<300 && !result.flinched;++t) {
    unsigned cx=s.camera_x+160,cy=s.camera_y+164;
    if(!hit) {
      /* Idle (behavior $0C step 1) with a long wait: no charge or volley. */
      g_ram[object+2]=0x0c;g_ram[object+3]=1;g_ram[object+0x39]=100;
      g_ram[object+5]=(uint8_t)cx;g_ram[object+6]=(uint8_t)(cx>>8);
      g_ram[object+8]=(uint8_t)cy;g_ram[object+9]=(uint8_t)(cy>>8);
      if(profile>=0) g_ram[object+0x28]=(uint8_t)profile;
    }
    g_ram[penguin+5]=(uint8_t)(s.camera_x-128);g_ram[penguin+6]=(uint8_t)((s.camera_x-128)>>8);
    g_ram[0xbad]=(uint8_t)(s.camera_x+100);g_ram[0xbae]=(uint8_t)((s.camera_x+100)>>8);
    g_ram[0xbb0]=(uint8_t)cy;g_ram[0xbb1]=(uint8_t)(cy>>8);
    g_ram[0xbaa]=g_ram[0xbab]=g_ram[0xc2d]=0;g_ram[0xbb9]&=~64u;g_ram[0xbdb]=(uint8_t)(page?0:weapon*2);
    for(unsigned d=0xe68;d<0x1628;d+=64) if(d<0x1228 || d>=0x1428) {
      if(MmxBossRushOwner(d)>=0 && g_ram[d]) g_ram[d+0x26]=0; /* no contact knockback */
    }
    for(unsigned d=0x1228;d<0x1428;d+=64) if(g_ram[d]) {
      memcpy(g_ram+d+5,g_ram+object+5,2);memcpy(g_ram+d+8,g_ram+object+8,2);
      g_ram[d+4]=g_ram[d+7]=0;
    }
    /* Pack shots follow their combat record; keep those on the target too. */
    MmxWeaponCombatState c=MmxWeaponsGetCombatState();
    for(unsigned i=0;i<8;++i) if(c.shots[i].active) {
      c.shots[i].x=(int32_t)((g_ram[object+5]|g_ram[object+6]<<8)<<8);
      c.shots[i].y=(int32_t)((g_ram[object+8]|g_ram[object+9]<<8)<<8);
    }
    MmxWeaponsSetCombatState(c);
    frame(t%90==0 && !hit?SNES_PAD_Y:0);rush_keep_players_alive();
    unsigned hp=g_ram[object+0x27]&127;
    if(!hit && hp<start_hp) {hit=true;result.damage=start_hp-hp;}
    if(hit) {
      result.profile=g_ram[object+0x28];
      if(g_ram[object+1]==2 && (g_ram[object+2]==8 || g_ram[object+2]==10)) result.flinched=true;
      if(t>200) break;
    }
    if(!hit && profile==0 && t>=120) break; /* deflected: never damaged */
  }
  return result;
}
static void rush_rhino_weakness_checks(uint8_t *snapshot,size_t size,unsigned object,const char *x3) {
  RhinoHit buster=rush_rhino_shoot(snapshot,size,object,0,0,-1);
  RhinoHit torpedo=rush_rhino_shoot(snapshot,size,object,0,1,-1);
  RhinoHit deflect=rush_rhino_shoot(snapshot,size,object,0,0,0);
  /* Acid Burst needs the X3 weapon pack, built from the same X3 ROM. */
  char cache[512],error[512];
  check(snesrecomp_exe_dir_path("rhino-x3-weapons.bin",cache,sizeof(cache)) &&
      MmxSourceAssetsBuild(x3,3,0,cache,error,sizeof(error)) && MmxWeaponsLoadPage(cache,2),
      "the X3 weapon pack loads for Rhino's weakness");
  MmxZeroRegisterHooks();
  RhinoHit acid=rush_rhino_shoot(snapshot,size,object,2,1,-1);
  MmxWeaponsCancelShots(g_ram);MmxWeaponsDisable();
  fprintf(stderr,"rhino hits: buster=%u/%u torpedo=%u/%u acid=%u/%u row=%u deflect=%u\n",
      buster.damage,buster.flinched,torpedo.damage,torpedo.flinched,acid.damage,acid.flinched,
      acid.profile,deflect.damage);
  check(buster.damage==1 && !buster.flinched,"the buster deals Rhino's ordinary damage");
  check(torpedo.damage==1 && !torpedo.flinched,"Homing Torpedo is not one of Rhino's weaknesses");
  check(acid.damage==3 && acid.flinched,"Acid Burst is Rhino's weakness and makes him flinch");
  check(acid.profile==5,"Rhino's weakness flinch is invulnerable");
  check(!deflect.damage,"his drill-charge row deflects shots");
}
/* Tunnel Rhino's original X3 code, relocated from the user's ROM, fights in
 * seat 1 beside a native Penguin. MMX_BOSS_RUSH_X3_ROM names the X3 ROM. */
static void rush_rhino_checks(uint8_t *start,size_t cap,const char *x3) {
  char error[256];
  check(x3 && MmxBossRushForeignLoad(3,x3,error,sizeof(error)),"X3 ROM loads for the Rhino fixture");
  check(MmxBossRushForeignBuild(error,sizeof(error)),"Rhino relocates into the cart padding");
  if(error[0]) fprintf(stderr,"rhino build: %s\n",error);
  check(MmxBossRushForeignController(23)==0xbfe5e4 && MmxBossRushForeignChild(23,0x26)==0xb8ab9b,
      "Rhino body and drill controllers register");
  static const uint8_t effects[]={0x15,0x21,0x49,0x70,0x72,0x73};
  for(unsigned i=0;i<sizeof(effects);++i) {
    uint32_t frames=0;const int16_t *clip=MmxBossRushAudioForeignClip(23,effects[i],&frames);
    int peak=0;
    for(uint32_t k=0;clip && k<frames*2;++k) if(abs(clip[k])>peak) peak=abs(clip[k]);
    fprintf(stderr,"rhino effect $%02x frames=%u (%.2fs) peak=%d\n",effects[i],frames,frames/32040.0,peak);
    check(clip && frames>=320 && peak>=256,"Rhino's X3 effects render from his own driver and bank");
    if(clip && getenv("MMX_BOSS_RUSH_RHINO_WAV")) {
      /* Private listening copies of the rendered clips (32040 Hz stereo). */
      char path[32];snprintf(path,sizeof(path),"rhino-%02x.wav",effects[i]);
      FILE *w=fopen(path,"wb");check(w!=NULL,"Rhino effect WAV opens");
      uint32_t bytes=frames*4,rate=32040,byte_rate=rate*4,chunk=36+bytes,fmt_size=16;
      uint16_t format=1,channels=2,align=4,bits=16;
      fwrite("RIFF",1,4,w);fwrite(&chunk,4,1,w);fwrite("WAVEfmt ",1,8,w);fwrite(&fmt_size,4,1,w);
      fwrite(&format,2,1,w);fwrite(&channels,2,1,w);fwrite(&rate,4,1,w);fwrite(&byte_rate,4,1,w);
      fwrite(&align,2,1,w);fwrite(&bits,2,1,w);fwrite("data",1,4,w);fwrite(&bytes,4,1,w);
      fwrite(clip,4,frames,w);check(!fclose(w),"Rhino effect WAV writes");
    }
  }
  check(!MmxBossRushAudioForeignClip(23,0x5a,NULL),"only Rhino's own effects are cached");
  check(!(MmxBossRushOffered()&(1u<<23)),"unfinished Rhino stays out of normal runs");
  MmxBossRushForeignSetPreview(true);
  check(MmxBossRushOffered()&(1u<<23),"developer preview offers the relocated Rhino");
  MmxBossRushState s=MmxBossRushGetState();
  memset(s.bosses,0,sizeof(s.bosses));memset(s.owner,0,sizeof(s.owner));
  memset(g_ram+0xe68,0,0x3c0);memset(g_ram+0x1428,0,0x8e0);s.queued=0;
  check(MmxBossRushSetState(&s),"Rhino fixture clears the opening pair");
  MmxBossRushRosterState roster={.available=MMX_RUSH_X1_MASK|(1u<<23),.queued=2};
  roster.queue[0]=0;roster.queue[1]=23;
  check(MmxBossRushRosterSetState(&roster),"Rhino fixture queues Penguin and Rhino");
  unsigned object=0;
  for(unsigned t=0;t<600 && !object;++t) {
    frame(0);rush_keep_players_alive();s=MmxBossRushGetState();
    if(s.bosses[1].phase && s.bosses[1].id==23) object=s.bosses[1].object;
  }
  check(object!=0,"Rhino spawns in seat 1");
  unsigned seen_behavior=0,drill_frames=0,min_x=0xffff,max_x=0,max_hp=0,last=0xffffff,fighting_at=0;
  uint32_t cue_sequence=MmxBossRushAudioGetState().sequence,heard[8]={0};
  for(unsigned t=0;t<1800;++t) {
    frame(0);rush_keep_players_alive();s=MmxBossRushGetState();
    MmxBossRushAudioState audio=MmxBossRushAudioGetState();
    for(;cue_sequence!=audio.sequence;++cue_sequence) {
      const MmxBossRushAudioCue *cue=&audio.cues[cue_sequence%MMX_RUSH_AUDIO_CUES];
      if(cue->reserved==23) heard[cue->command>>5]|=1u<<(cue->command&31);
    }
    if(!s.bosses[1].phase || s.bosses[1].id!=23) break;
    unsigned state=g_ram[object+1]<<16|g_ram[object+2]<<8|g_ram[object+3];
    unsigned x=eagle_word(g_ram+object+5);
    if(state!=last && (t<600 || (g_ram[object+2]>=2 && g_ram[object+2]<=6))) fprintf(stderr,"rhino t=%u state=%02x/%02x/%02x x=%04x y=%04x hp=%u anim=%02x/%02x live=%u\n",
        t,state>>16,(state>>8)&255,state&255,x,eagle_word(g_ram+object+8),g_ram[object+0x27]&127,
        g_ram[object+0x0f],g_ram[object+0x13],g_ram[object+0x3d]);
    last=state;
    if(getenv("MMX_BOSS_RUSH_RHINO_ANIM") && g_ram[object+1]==2 && g_ram[object+2]==4) {
      unsigned record=eagle_word(g_ram+object+0x14);
      const uint8_t *at=RomPtr(0x3f0000|record);
      fprintf(stderr,"anim t=%u set=%02x step=%02x f=%02x timer=%02x rec=%04x [%02x %02x %02x] pose=%02x\n",
          t,g_ram[object+0x16],g_ram[object+3],g_ram[object+0x0f],g_ram[object+0x13],record,at[0],at[1],at[2],g_ram[object+0x17]);
    }
    if(g_ram[object+1]==2) {
      seen_behavior|=1u<<(g_ram[object+2]/2);
      if(x<min_x) min_x=x;
      if(x>max_x) max_x=x;
      if(!fighting_at) fighting_at=t;
    }
    if((g_ram[object+0x27]&127)>max_hp) max_hp=g_ram[object+0x27]&127;
    unsigned live_drills=0,drill=0;
    for(unsigned d=0x1428;d<0x1628;d+=64)
      if(g_ram[d] && g_ram[d+10]==0x26 && MmxBossRushOwner(d)==1) {++drill_frames;++live_drills;drill=d;}
    static unsigned first_drill;
    if(live_drills && !first_drill) first_drill=t;
    static uint32_t image[256*224];
    if(t==fighting_at+40 && fighting_at) {
      check(MmxRendererDraw(image,(MmxRenderView){256,0,4.0/3.0},true),"Rhino combat frame renders");
      unsigned art=rush_rhino_art_pixels(image,object,23);
      fprintf(stderr,"rhino art pixels=%u at t=%u\n",art,t);
      check(art>=400,"Rhino draws with his own X3 art and palette");
      rush_rhino_save_ppm("rush-rhino-combat.ppm",image);
    }
    if(first_drill && t==first_drill+40 && drill) {
      check(MmxRendererDraw(image,(MmxRenderView){256,0,4.0/3.0},true),"Rhino drill frame renders");
      unsigned art=rush_rhino_art_pixels(image,drill,23);
      const MmxForeignPose *dp=MmxBossRushForeignPose(23,g_ram[drill+0x16],g_ram[drill+0x17]&0x7f);
      fprintf(stderr,"rhino drill art pixels=%u at t=%u drills=%u obj=%04x active=%02x set=%02x pose=%02x xy=%04x,%04x cam=%04x,%04x attr=%02x on=%02x layout=%s\n",
          art,t,live_drills,drill,g_ram[drill],g_ram[drill+0x16],g_ram[drill+0x17],eagle_word(g_ram+drill+5),
          eagle_word(g_ram+drill+8),eagle_word(g_ram+0x1e4d),eagle_word(g_ram+0x1e50),g_ram[drill+0x11],
          g_ram[drill+0x0e],dp?"ok":"none");
      check(g_ram[drill+0x16]==g_ram[object+0x16],"Rhino's drills run their relocated X3 controller");
      check(art>=40,"Rhino's drills draw with his own X3 art");
      rush_rhino_save_ppm("rush-rhino-drills.ppm",image);
    }
  }
  s=MmxBossRushGetState();
  fprintf(stderr,"rhino fighting_at=%u behaviors=%03x x=%04x..%04x hp=%u drill-frames=%u phase=%u penguin=%u\n",
      fighting_at,seen_behavior,min_x,max_x,max_hp,drill_frames,s.bosses[1].phase,s.bosses[0].phase);
  check(fighting_at && s.bosses[1].phase==MMX_RUSH_FIGHTING,"Rhino's original intro hands over to combat");
  check(max_hp==32,"Rhino's intro fills his native health meter");
  check(max_x-min_x>=64,"Rhino moves through the arena");
  unsigned behaviors=0;
  for(unsigned b=seen_behavior;b;b&=b-1) ++behaviors;
  check(behaviors>=2,"Rhino chooses between original behaviors");
  check(s.bosses[0].id==0 && s.bosses[0].phase,"native Penguin keeps fighting beside Rhino");
  const uint8_t *row=MmxBossRushForeignRom(3,NULL)+0x30000+(0xe28e&0x7fff)+(0x55-1)*5;
  check(g_ram[object+0x16]==row[0] && g_ram[object+0x16],"Rhino uses his own X3 sprite set");
  check(drill_frames>0,"Rhino's drill volley spawns relocated drill projectiles");
  #define RHINO_HEARD(c) ((heard[(c)>>5]>>((c)&31))&1)
  fprintf(stderr,"rhino cues: fill=%u crash=%u spread=%u stop=%u grind=%u\n",RHINO_HEARD(0x15),
      RHINO_HEARD(0x49),RHINO_HEARD(0x70),RHINO_HEARD(0x72),RHINO_HEARD(0x73));
  check(RHINO_HEARD(0x15) && RHINO_HEARD(0x49),"Rhino's intro fill and wall crash queue his own X3 sounds");
  check(RHINO_HEARD(0x73),"his drills queue their grinding sound");
  size_t n=RtlSaveSnapshotToMemory(start,cap);check(n,"Rhino seat snapshot saves");
  static uint8_t first[0x20000];
  for(unsigned t=0;t<60;++t) {frame(0);rush_keep_players_alive();}
  memcpy(first,g_ram,sizeof(first));
  check(RtlLoadSnapshotFromMemory(start,n),"Rhino seat snapshot restores");
  for(unsigned t=0;t<60;++t) {frame(0);rush_keep_players_alive();}
  check(!memcmp(first,g_ram,sizeof(first)),"Rhino replay is deterministic");
  rush_rhino_weakness_checks(start,n,object,x3);
  check(RtlLoadSnapshotFromMemory(start,n),"Rhino seat snapshot restores for defeat");
  unsigned defeated=MmxBossRushGetState().defeated;
  uint32_t generation=MmxBossRushGetState().bosses[1].generation;
  g_ram[object+0x27]=0;frame(0);s=MmxBossRushGetState();
  check(s.defeated==defeated+1 && s.bosses[1].phase==MMX_RUSH_DYING,"Rush counts Rhino's defeat");
  for(unsigned t=0;t<90;++t) {frame(0);rush_keep_players_alive();}
  s=MmxBossRushGetState();
  bool leftovers=false;
  for(unsigned d=0x1428;d<0x1628;d+=64)
    if(g_ram[d] && g_ram[d+10]==0x26 && MmxBossRushOwner(d)==1) leftovers=true;
  /* The replacement may be Rhino again: the roster only excludes bosses
   * still present. Require a new generation instead of a different id. */
  check(!leftovers && s.bosses[1].phase && s.bosses[1].generation!=generation,
      "Rhino and his drills are cleaned up and replaced");
  MmxBossRushForeignUnload();MmxBossRushForeignSync();MmxBossRushForeignSetPreview(false);
  check(!MmxBossRushForeignController(23) && !(MmxBossRushOffered()&(1u<<23)),
      "unloading the X3 pack removes Rhino");
  puts("MMX BOSS RUSH RHINO CHECKS PASSED");
}
