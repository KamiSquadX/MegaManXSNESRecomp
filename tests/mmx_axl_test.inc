/* MMX_AXL_TEST: Axl's moveset on X's body in the fully upgraded Boss Rush
 * preparation corridor. MMX_AXL_FIXTURE names a snapshot cache: it is
 * written on the first run and loaded afterwards.
 * Probes: MMX_AXL_PROBE="count:hexinput,..." logs player RAM per frame
 * (bit 24 forces a charge release, bit 25 sets $0C13, bit 26 holds ARMOR);
 * MMX_AXL_CAPTURE=<prefix> writes a BMP per probe frame. */
static unsigned axl_x(void) { return g_ram[0xbad] | g_ram[0xbae] << 8; }
static unsigned axl_y(void) { return g_ram[0xbb0] | g_ram[0xbb1] << 8; }
static void axl_title_and_corridor(const char *fixture) {
  bool booted=!(fixture && RtlLoadSnapshot(fixture));
  if(!booted) return;
  for(unsigned t=0;t<1800 && !MmxBossRushGetState().menu;++t)
    frame(t==200 || t==350 || (t>500 && t%180==0)?SNES_PAD_START:0);
  check(MmxBossRushGetState().menu==1,"title menu appears");
  zero_replay(180);
  for(unsigned n=0;n<4;++n) {frame(SNES_PAD_DOWN);zero_replay(12);}
  frame(SNES_PAD_Y);frame(SNES_PAD_Y);
  for(unsigned t=0;t<1800 && MmxBossRushGetState().mode!=MMX_RUSH_PREPARING;++t) frame(SNES_PAD_Y);
  check(MmxBossRushGetState().mode==MMX_RUSH_PREPARING,"preparation corridor reached");
  zero_replay(120);
  if(fixture) check(RtlSaveSnapshot(fixture),"corridor fixture saves");
}
static void axl_image(unsigned t) {
  const char *base=getenv("MMX_AXL_CAPTURE");
  if(!base) return;
  int w,h;MmxPrepareFrame(1280,720,&w,&h);
  uint32_t pixels[256*224];
  if(!MmxRendererDraw(pixels,(MmxRenderView){256,0,4.0/3.0},false)) return;
  char path[4096];snprintf(path,sizeof(path),"%s%04u.bmp",base,t);
  FILE *f=fopen(path,"wb");if(!f) return;
  uint8_t signature[2]={'B','M'};
  uint32_t header[]={54+sizeof(pixels),0,54,40,256,224,0x200001,0,sizeof(pixels),0,0,0,0};
  fwrite(signature,2,1,f);fwrite(header,sizeof(header),1,f);
  for(int y=223;y>=0;--y) fwrite(pixels+y*256,1024,1,f);
  fclose(f);
}
static void axl_probe(void) {
  static uint8_t before[0x2000];
  memcpy(before,g_ram,sizeof(before));
  const char *script=getenv("MMX_AXL_PROBE");
  unsigned t=0;
  for(const char *p=script;p && *p;) {
    unsigned count=0,input=0;
    if(sscanf(p,"%u:%x",&count,&input)!=2) break;
    for(unsigned i=0;i<count;++i,++t) {
      if(input&0x2000000u) g_ram[0xc13]=2;
      if(input&0x1000000u) {g_ram[0xbff]=0x50;g_ram[0xc00]=4;g_ram[0xc02]=1;g_ram[0xc03]=3;g_ram[0xbe1]|=0x40;g_ram[0xbdf]|=0x40;}
      RtlSetPadExt(0,(input&0x4000000u)?1:0);
      frame(input&0xffffffu);
      if(getenv("MMX_AXL_CAPTURE")) axl_image(t);
      fprintf(stderr,"t=%3u in=%03x st=%02x/%02x grp=%02x pose=%02x xy=%u,%u vy=%04x hover=%u dash=%u giga=%u\n",
          t,input,g_ram[0xbaa],g_ram[0xbab],g_ram[0xbbe],g_ram[0xbbf],axl_x(),axl_y(),
          g_ram[0xbc4]|g_ram[0xbc5]<<8,MmxAxlGetState().hover,MmxAxlGetState().dash,MmxAxlGetState().giga);
    }
    p=strchr(p,',');if(!p) break;++p;
  }
  for(unsigned a=0xba8;a<0xc40;++a) if(g_ram[a]!=before[a])
    fprintf(stderr,"diff %04x %02x->%02x\n",a,before[a],g_ram[a]);
}
static unsigned axl_new_slot(uint8_t before) {
  for(unsigned i=0;i<8;++i) if(g_ram[0x1228+i*64] && !(before&(1u<<i))) return 0x1228+i*64;
  return 0;
}
static uint8_t axl_live_slots(void) {
  uint8_t live=0;for(unsigned i=0;i<8;++i) if(g_ram[0x1228+i*64]) live|=1u<<i;
  return live;
}
/* Each of the 8 aims, on the ground and in the air: the shot's travel must
 * point the way the d-pad does. */
static void axl_aim_checks(const uint8_t *base,size_t base_n) {
  static const unsigned dirs[8]={SNES_PAD_RIGHT,SNES_PAD_RIGHT|SNES_PAD_UP,SNES_PAD_UP,
      SNES_PAD_LEFT|SNES_PAD_UP,SNES_PAD_LEFT,SNES_PAD_LEFT|SNES_PAD_DOWN,SNES_PAD_DOWN,
      SNES_PAD_RIGHT|SNES_PAD_DOWN};
  static const int sx[8]={1,1,0,-1,-1,-1,0,1},sy[8]={0,-1,-1,-1,0,1,1,1};
  for(unsigned air=0;air<2;++air) for(unsigned k=0;k<8;++k) {
    check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
    if(air) for(unsigned t=0;t<8;++t) frame(SNES_PAD_B);
    unsigned hold=air?SNES_PAD_B:0;
    uint8_t before=axl_live_slots();
    frame(hold|SNES_PAD_Y|dirs[k]);
    unsigned slot=axl_new_slot(before);
    check(slot!=0,"an aimed shot fires");
    int x0=g_ram[slot+5]|g_ram[slot+6]<<8,y0=g_ram[slot+8]|g_ram[slot+9]<<8;
    for(unsigned t=0;t<3;++t) frame(hold|SNES_PAD_Y|dirs[k]);
    int dx=(g_ram[slot+5]|g_ram[slot+6]<<8)-x0,dy=(g_ram[slot+8]|g_ram[slot+9]<<8)-y0;
    fprintf(stderr,"axl aim air=%u dir=%u: dx=%d dy=%d\n",air,k,dx,dy);
    bool ok=(sx[k]>0?dx>4:sx[k]<0?dx<-4:dx==0) && (sy[k]>0?dy>4:sy[k]<0?dy<-4:dy==0);
    check(ok,air?"the air shot travels the way Axl aims":"the ground shot travels the way Axl aims");
  }
}
static unsigned axl_escape_hits;
static void axl_escape_logger(CpuState *cpu,uint32_t pc) {(void)cpu;(void)pc;++axl_escape_hits;}
/* MMX_AXL_ESCAPE_TEST: which pause-menu row reaches the Escape Unit check. */
/* MMX_AXL_ESCAPE_TEST: ESCAPE.U ends the run with the Retry/Main Menu
 * screen; Retry starts a new run. */
static void axl_pause_trace(const uint8_t *base,size_t base_n) {
  static uint8_t a[0x2000],b[0x2000],c[0x2000];
  RtlLoadSnapshotFromMemory(base,base_n);zero_replay(10);memcpy(a,g_ram,sizeof(a));
  frame(SNES_PAD_START);zero_replay(60);memcpy(b,g_ram,sizeof(b));
  frame(SNES_PAD_START);zero_replay(60);memcpy(c,g_ram,sizeof(c));
  for(unsigned i=0x30;i<0x2000;++i) if(a[i]==c[i] && b[i]!=a[i])
    fprintf(stderr,"pause %04x: %02x open=%02x closed=%02x\n",i,a[i],b[i],c[i]);
}
static void axl_escape_probe(const uint8_t *base,size_t base_n) {
  if(getenv("MMX_AXL_PAUSE_TRACE")) {axl_pause_trace(base,base_n);return;}
  for(unsigned retry=0;retry<2;++retry) {
    RtlLoadSnapshotFromMemory(base,base_n);
    frame(SNES_PAD_START);zero_replay(60);
    frame(SNES_PAD_RIGHT);zero_replay(8);
    for(unsigned k=0;k<4;++k) {frame(SNES_PAD_DOWN);zero_replay(8);}
    frame(SNES_PAD_START);
    fprintf(stderr,"escape: flag=%u menu=%u\n",MmxBossRushScratchGetState().escape,MmxWeaponsMenuVisible(g_ram));
    unsigned t=0;
    if(getenv("MMX_AXL_ESCAPE_TRACE")) {
      static uint8_t prev[0x2000];memcpy(prev,g_ram,sizeof(prev));
      for(unsigned k=0;k<200;++k) {
        frame(0);
        fprintf(stderr,"esc t=%u:",k);
        for(unsigned a=0x30;a<0x2000;++a) if(g_ram[a]!=prev[a] && (a<0xb00 || a>=0x1f00 || (a>=0xba8 && a<0xc40)) && a!=0xb9b && a!=0xba6)
          fprintf(stderr," %04x=%02x",a,g_ram[a]);
        fputc(10,stderr);memcpy(prev,g_ram,sizeof(prev));
      }
      return;
    }
    for(;t<600 && MmxBossRushGetState().mode!=MMX_RUSH_FINISHED;++t) frame(0);
    fprintf(stderr,"escape: finished after %u frames mode=%u action=%02x/%02x\n",t,MmxBossRushGetState().mode,g_ram[0xbaa],g_ram[0xbab]);
    check(MmxBossRushGetState().mode==MMX_RUSH_FINISHED,"ESCAPE.U opens the Boss Rush result screen");
    check(t>60,"ESCAPE.U first plays the escape chime and closes the menu");
    for(unsigned k=0;k<60;++k) {frame(0);if(getenv("MMX_AXL_SCENE_TRACE") && k%10==0) fprintf(stderr,"scene finished+%u d1=%02x %02x %02x %02x b3=%02x\n",k,g_ram[0xd1],g_ram[0xd2],g_ram[0xd3],g_ram[0xd4],g_ram[0xb3]);}
    axl_image(8200+retry);
    if(retry) {frame(SNES_PAD_DOWN);zero_replay(4);}
    frame(SNES_PAD_Y);
    for(t=0;t<2400;++t) {
      frame(0);
      if(getenv("MMX_AXL_SCENE_TRACE") && t%20==0) fprintf(stderr,"scene after+%u d1=%02x %02x %02x %02x mode=%u\n",t,g_ram[0xd1],g_ram[0xd2],g_ram[0xd3],g_ram[0xd4],MmxBossRushGetState().mode);
      unsigned m=MmxBossRushGetState().mode;
      if(!retry && m==MMX_RUSH_PREPARING) break;
      if(retry && MmxBossRushGetState().menu) break;
    }
    fprintf(stderr,"escape: after %s %u frames mode=%u menu=%u\n",retry?"main menu":"retry",t,
        MmxBossRushGetState().mode,MmxBossRushGetState().menu);
    if(!retry) check(MmxBossRushGetState().mode==MMX_RUSH_PREPARING && (g_ram[0xbcf]&127),"Retry starts a new run");
    else check(MmxBossRushGetState().menu!=0,"Main Menu returns to the title");
  }
  puts("MMX AXL ESCAPE CHECKS PASSED");
}
/* MMX_AXL_LAUNCHER_ROOT=<copy of mods/preloaded>: the launcher's rules for
 * Add Zero, Add Axl, Solo Team select and the co-op modes. */
static bool axl_feature_on(const RecompLauncherCModProvider *p,const char *package,const char *feature) {
  for(int i=0;i<p->feature_count(p->ctx);++i) {
    RecompLauncherCModFeature f;memset(&f,0,sizeof(f));
    if(p->feature_get(p->ctx,i,&f) && !strcmp(f.package_id,package) && !strcmp(f.id,feature))
      return f.enabled!=0;
  }
  return false;
}
static int axl_choice_disabled(const RecompLauncherCModProvider *p,const char *package,const char *feature,
                               const char *option,const char *value) {
  for(int i=0;i<8;++i) {
    RecompLauncherCModChoice c;memset(&c,0,sizeof(c));
    if(!p->feature_choice_get(p->ctx,package,feature,option,i,&c)) break;
    if(!strcmp(c.value,value)) return c.disabled;
  }
  return -1;
}
static const char *axl_option_value(const RecompLauncherCModProvider *p,const char *package,const char *feature,
                                    const char *option) {
  static RecompLauncherCModOption o;
  for(int i=0;i<16;++i) {
    memset(&o,0,sizeof(o));
    if(!p->feature_option_get(p->ctx,package,feature,i,&o)) break;
    if(!strcmp(o.id,option)) return o.value;
  }
  return "";
}
static void axl_launcher_checks(const char *root) {
  check(snes_mod_runtime_initialize_c(root,"megaman-x-us",
      "b8f70a6e7fb93819f79693578887e2c11e196bdf1ac6ddc7cb924b1ad0be2d32"),"catalog parses");
  const RecompLauncherCModProvider *p=snes_mod_runtime_launcher_provider_c();
  const char *zero="megaman-x.character.zero",*axl="megaman-x.character.axl",
      *team="megaman-x.team-select",*coop="megaman-x.coop-axl";
  p->feature_enable(p->ctx,team,"team",1);
  check(!axl_feature_on(p,team,"team"),"Solo Team select stays off without Add Zero or Add Axl");
  check(p->feature_enable(p->ctx,zero,"zero",1),"Add Zero enables");
  check(axl_feature_on(p,team,"team"),"Add Zero turns on Solo Team select");
  p->feature_enable(p->ctx,team,"team",0);
  check(axl_feature_on(p,team,"team"),"Solo Team select cannot be turned off while Add Zero is on");
  check(axl_choice_disabled(p,team,"team","start","axl")==1 &&
      axl_choice_disabled(p,team,"team","partner","axl")==1 &&
      axl_choice_disabled(p,team,"team","start","x")==0,"without Add Axl, Axl is greyed out in both lists");
  check(p->feature_enable(p->ctx,axl,"axl",1),"Add Axl enables");
  check(axl_feature_on(p,zero,"zero") && axl_feature_on(p,axl,"axl") && axl_feature_on(p,team,"team"),
      "Add Zero, Add Axl and Solo Team select are on together");
  check(p->feature_set_option(p->ctx,team,"team","start","axl"),"Axl starts");
  check(axl_choice_disabled(p,team,"team","partner","axl")==1 &&
      axl_choice_disabled(p,team,"team","partner","zero")==0 &&
      axl_choice_disabled(p,team,"team","partner","none")==0,"the starting character is greyed out for the partner");
  check(p->feature_enable(p->ctx,zero,"zero",0) && axl_feature_on(p,team,"team") &&
      axl_choice_disabled(p,team,"team","partner","zero")==1,"Add Axl alone keeps Solo Team select, without Zero");
  check(p->feature_enable(p->ctx,axl,"axl",0) && !axl_feature_on(p,team,"team"),
      "Solo Team select turns off with Add Zero and Add Axl");
  check(p->feature_enable(p->ctx,zero,"zero",1) && p->feature_enable(p->ctx,axl,"axl",1),"both characters return");
  /* A character turned off leaves the team: start to X, partner to None. */
  check(p->feature_set_option(p->ctx,team,"team","start","axl") &&
      p->feature_set_option(p->ctx,team,"team","partner","zero"),"an Axl and Zero team");
  p->feature_enable(p->ctx,axl,"axl",0);
  fprintf(stderr,"axl team without Axl: %s/%s\n",axl_option_value(p,team,"team","start"),
      axl_option_value(p,team,"team","partner"));
  check(!strcmp(axl_option_value(p,team,"team","start"),"x") &&
      !strcmp(axl_option_value(p,team,"team","partner"),"zero"),"turning off Axl moves the start to X");
  p->feature_enable(p->ctx,axl,"axl",1);
  check(!strcmp(axl_option_value(p,team,"team","start"),"x"),"the corrected start stays when Axl returns");
  p->feature_enable(p->ctx,zero,"zero",0);p->feature_enable(p->ctx,axl,"axl",0);
  check(!strcmp(axl_option_value(p,team,"team","start"),"x") &&
      !strcmp(axl_option_value(p,team,"team","partner"),"none"),"with neither character the team is X and None");
  check(p->feature_enable(p->ctx,zero,"zero",1) && p->feature_enable(p->ctx,axl,"axl",1),"both characters return again");
  check(axl_choice_disabled(p,coop,"coop","player1","x")==0 &&
      axl_choice_disabled(p,coop,"coop","player2","x")==1 &&
      axl_choice_disabled(p,coop,"coop","player2","zero")==0 &&
      axl_choice_disabled(p,coop,"coop","player2","axl")==0 &&
      axl_choice_disabled(p,coop,"coop","player1","zero")==1,"co-op lists all three characters");
  check(p->feature_enable(p->ctx,coop,"coop",1),"Co-op 0.0.2 enables");
  check(!axl_feature_on(p,zero,"zero") && !axl_feature_on(p,axl,"axl") && !axl_feature_on(p,team,"team"),
      "Co-op turns off Add Zero, Add Axl and Solo Team select");
  puts("MMX AXL LAUNCHER CHECKS PASSED");
}
/* MMX_AXL_HIGHWAY: Game Start, then rapid fire through the intro stage's
 * enemies, logging the player's projectile slots. */
static void axl_highway(void) {
  for(unsigned t=0;t<1800 && !MmxBossRushGetState().menu;++t)
    frame(t==200 || t==350 || (t>500 && t%180==0)?SNES_PAD_START:0);
  zero_replay(60);
  frame(SNES_PAD_START);
  for(unsigned t=0;t<1500 && !(g_ram[0xd1]==2 && g_ram[0xd2]==4 && g_ram[0xd3]==4 && g_ram[0xba9]==2);++t) {
    frame(t%40==0?SNES_PAD_START:0);
  }
  /* The beam-in: X's column and landing morphs are Axl's beam and intro,
   * and X's own art never shows. */
  unsigned beam=0,landing=0,shown=0,steps=0;
  for(unsigned t=0;t<120;++t) {
    frame(0);
    MmxRenderAxlFrame out;
    bool drawn=MmxAxlFrame(g_ram,&out);
    MmxAxlState s=MmxAxlGetState();
    if(g_ram[0xbaa]!=0x22) continue;
    unsigned pose=g_ram[0xbbf]&127;
    if(pose==0x3c) beam+=s.anim==MMX_AXL_ANIM_BEAM;
    if(pose>0x3c && pose<=0x42 && s.anim==MMX_AXL_ANIM_BEAM_IN) {++landing;steps|=1u<<s.anim_step;}
    shown+=drawn && out.own_body && out.body.active;
  }
  fprintf(stderr,"highway beam=%u landing=%u steps=%03x shown=%u\n",beam,landing,steps,shown);
  check(beam>0 && landing>0 && (steps&0x200) && shown>=beam+landing,"Axl beams in as X8's intro");
  fprintf(stderr,"highway: stage=%u d1=%02x/%02x/%02x x=%u\n",g_ram[0x1f7a],g_ram[0xd1],g_ram[0xd2],g_ram[0xd3],axl_x());
  if(getenv("MMX_AXL_FIXTURE_HIGHWAY")) RtlSaveSnapshot(getenv("MMX_AXL_FIXTURE_HIGHWAY"));
  unsigned last_fire=0;uint8_t seen=axl_live_slots();
  for(unsigned t=0;t<6000;++t) {
    unsigned in=SNES_PAD_Y|((t/240)%3==2?0:SNES_PAD_RIGHT)|((t/60)%5==3?SNES_PAD_B:0)|((t/120)%4==1?SNES_PAD_UP:0);
    frame(in);g_ram[0xbcf]=(uint8_t)((g_ram[0xbcf]&128)|16);
    uint8_t now=axl_live_slots();
    if(now&~seen) last_fire=t;
    seen=now;
    if(t%200==0 || t-last_fire==60) {
      fprintf(stderr,"highway t=%u x=%u bdd=%u act=%02x last=%u slots:",t,axl_x(),g_ram[0xbdd],g_ram[0xbaa],last_fire);
      for(unsigned i=0;i<8;++i) {unsigned d=0x1228+i*64;
        if(g_ram[d]) fprintf(stderr," [%u k%02x s%02x x%u y%u]",i,g_ram[d+10],g_ram[d+1],g_ram[d+5]|g_ram[d+6]<<8,g_ram[d+8]|g_ram[d+9]<<8);}
      fprintf(stderr," cam=%u,%u\n",g_ram[0x1e4d]|g_ram[0x1e4e]<<8,g_ram[0x1e50]|g_ram[0x1e51]<<8);
    }
  }
  check(last_fire>5800,"rapid fire keeps firing through the Highway");
  /* The partner's buster still has shot slots afterwards. */
  zero_replay(30);
  frame(SNES_PAD_SELECT);frame(0);zero_replay(10);
  uint8_t before=axl_live_slots();
  frame(SNES_PAD_Y);frame(0);
  check(!MmxAxlActive() && axl_new_slot(before),"X still fires after Axl's long rapid fire");
  puts("MMX AXL HIGHWAY CHECKS PASSED");
}
/* Rapid fire with each special weapon, then R must still change weapons. */
static void axl_weapon_swap_checks(const uint8_t *base,size_t base_n) {
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  bool stuck=false;
  for(unsigned w=0;w<9;++w) {
    unsigned from=g_ram[0xbdb];
    for(unsigned t=0;t<120;++t) frame(SNES_PAD_Y|(t%60<30?SNES_PAD_LEFT:SNES_PAD_RIGHT));
    /* X1 changes weapons only once its own count of live shots is zero. A
     * native shot that cannot leave this walled room keeps it honestly. */
    for(unsigned t=0;t<240 && g_ram[0xbdd];++t) frame(0);
    bool native_left=false;
    for(unsigned i=0;i<8;++i) native_left|=g_ram[0x1228+i*64]!=0;
    check(g_ram[0xbdd]<8,"the live shot count never wraps");
    if(native_left) {
      fprintf(stderr,"axl swap weapon %u: a native shot stays in the room\n",from);
      for(unsigned i=0;i<8;++i) memset(g_ram+0x1228+i*64,0,64);
      g_ram[0xbdd]=0;
    }
    frame(SNES_PAD_R);zero_replay(12);
    fprintf(stderr,"axl swap weapon %u -> %u bdd=%u\n",from,g_ram[0xbdb],g_ram[0xbdd]);
    stuck|=g_ram[0xbdb]==from;
  }
  check(!stuck,"R changes weapons after rapid fire with every weapon");
}
/* MMX_AXL_CHARGE_PROBE: per weapon, what X's held full charge fires against
 * what Axl's SHOT2 fires. */
static void axl_slot_dump(const char *who,unsigned weapon) {
  fprintf(stderr,"charge %s weapon=%u c13=%02x:",who,weapon,g_ram[0xc13]);
  for(unsigned i=0;i<8;++i) {unsigned d=0x1228+i*64;
    if(g_ram[d]) fprintf(stderr," [k%02x s%02x/%02x]",g_ram[d+10],g_ram[d+1],g_ram[d+2]);}
  fputc(10,stderr);
}
static void axl_charge_probe(const uint8_t *base,size_t base_n) {
  static uint8_t shot[2][64];
  for(unsigned w=0;w<9;++w) {
    for(unsigned axl=0;axl<2;++axl) {
      RtlLoadSnapshotFromMemory(base,base_n);
      for(unsigned k=0;k<w;++k) {frame(SNES_PAD_R);zero_replay(12);}
      unsigned weapon=g_ram[0xbdb];
      if(getenv("MMX_AXL_NOARMS")) g_ram[0x1f99]&=(uint8_t)~2;
      MmxAxlState s=MmxAxlGetState();s.active=(uint8_t)axl;MmxAxlSetState(&s);
      if(!axl) {for(unsigned t=0;t<240;++t) {frame(SNES_PAD_Y);
          if(t==100 || t==160 || t==239) {fprintf(stderr,"charge obj w%u t%u:",weapon,t);
            for(unsigned d=0xc98;d<0xe18;d+=32) if(g_ram[d]) {fprintf(stderr," [%04x",d);for(unsigned k=0;k<32;++k) fprintf(stderr,"%s%02x",k%8?"":" ",g_ram[d+k]);fputc(']',stderr);}
            fputc(10,stderr);}}
        frame(0);}
      else {fprintf(stderr,"charge ram axl w%u:",weapon);for(unsigned a=0xbf0;a<0xc10;++a) fprintf(stderr," %02x",g_ram[a]);fputc(10,stderr);frame(SNES_PAD_X);}
      zero_replay(6);
      axl_slot_dump(axl?"axl":"x",weapon);
      memset(shot[axl],0,64);
      for(unsigned i=0;i<8;++i) if(g_ram[0x1228+i*64]) {memcpy(shot[axl],g_ram+0x1228+i*64,64);break;}
    }
    fprintf(stderr,"charge diff weapon %u:",w);
    for(unsigned k=0;k<64;++k) if(shot[0][k]!=shot[1][k] && k!=4 && k!=5 && k!=6 && k!=7 && k!=8 && k!=9)
      fprintf(stderr," +%02x x=%02x axl=%02x",k,shot[0][k],shot[1][k]);
    fputc(10,stderr);
  }
}
extern const uint8_t kMmxAxlEmbeddedGigaStart[], kMmxAxlEmbeddedGigaShot[];
extern const size_t kMmxAxlEmbeddedGigaStartSize, kMmxAxlEmbeddedGigaShotSize;
extern int stb_vorbis_decode_memory(const unsigned char *mem, int len, int *channels,
                                    int *sample_rate, short **output);
static void axl_sound_checks(void) {
  const uint8_t *data[2]={kMmxAxlEmbeddedGigaStart,kMmxAxlEmbeddedGigaShot};
  size_t size[2]={kMmxAxlEmbeddedGigaStartSize,kMmxAxlEmbeddedGigaShotSize};
  for(unsigned i=0;i<2;++i) {
    int channels=0,rate=0;short *pcm=NULL;
    int frames=stb_vorbis_decode_memory(data[i],(int)size[i],&channels,&rate,&pcm);
    fprintf(stderr,"axl giga sound %u: %d frames, %d ch, %d Hz\n",i,frames,channels,rate);
    check(frames>1000 && (channels==1 || channels==2) && rate>=8000,"GigaCrash sound decodes");
    free(pcm);
  }
}
static void axl_checks(void) {
  axl_sound_checks();
  if(getenv("MMX_AXL_HIGHWAY")) {
    MmxAxlSetTeam(MMX_AXL_CHAR_AXL,MMX_AXL_CHAR_X);MmxAxlEnable(true);MmxAxlInstallHooks(true);
    MmxRendererSetAxlProvider(MmxAxlFrame);axl_highway();return;
  }
  if(getenv("MMX_AXL_LAUNCHER_ROOT")) {axl_launcher_checks(getenv("MMX_AXL_LAUNCHER_ROOT"));return;}
  if(!getenv("MMX_AXL_X")) {
    MmxAxlSetTeam(MMX_AXL_CHAR_AXL,MMX_AXL_CHAR_X);
    MmxAxlEnable(true);
    check(MmxAxlEnabled(),"Axl sprites load");
    MmxAxlInstallHooks(true);
    MmxRendererSetAxlProvider(MmxAxlFrame);
  }
  const char *fixture=getenv("MMX_AXL_FIXTURE");
  {int w,h;g_config.widescreen=false;MmxPrepareFrame(1280,720,&w,&h);}
  if(!(fixture && RtlLoadSnapshot(fixture))) {
    /* Axl starts, so he stands at the title cursor and fires three
     * pistol shots instead of the charge shot. */
    for(unsigned t=0;t<1800 && !MmxBossRushGetState().menu;++t)
      frame(t==200 || t==350 || (t>500 && t%180==0)?SNES_PAD_START:0);
    zero_replay(30);
    MmxRenderAxlFrame f;
    check(MmxAxlFrame(g_ram,&f) && f.title && f.body.active,"Axl stands at the title cursor");
    axl_image(9000);
    if(fixture) {
      for(unsigned n=0;n<4;++n) {frame(SNES_PAD_DOWN);zero_replay(12);}
      frame(SNES_PAD_Y);frame(SNES_PAD_Y);
      bool shots=false;
      for(unsigned t=0;t<20;++t) {frame(SNES_PAD_Y);axl_image(9001+t);MmxAxlFrame(g_ram,&f);shots|=f.title && f.sprite_count>=3;}
      check(shots,"the title shot is three pistol shots");
      for(unsigned t=0;t<1800 && MmxBossRushGetState().mode!=MMX_RUSH_PREPARING;++t) frame(SNES_PAD_Y);
      check(MmxBossRushGetState().mode==MMX_RUSH_PREPARING,"preparation corridor reached");
      zero_replay(120);
      check(RtlSaveSnapshot(fixture),"corridor fixture saves");
    } else axl_title_and_corridor(NULL);
  }
  if(getenv("MMX_AXL_PROBE")) {axl_probe();return;}
  static uint8_t base[1<<20];
  size_t base_n=RtlSaveSnapshotToMemory(base,sizeof(base));
  check(base_n>0,"corridor snapshot saves");
  if(getenv("MMX_AXL_ESCAPE_TEST")) {axl_escape_probe(base,base_n);return;}
  if(getenv("MMX_AXL_CHARGE_PROBE")) {axl_charge_probe(base,base_n);return;}
  MmxRenderAxlFrame f;
  zero_replay(2);
  check(MmxAxlActive() && MmxAxlFrame(g_ram,&f) && f.body.active && f.body.body.pixels,
      "Axl replaces X's body");

  /* Rapid fire: a held SHOT fires every six frames, past X1's three-shot cap. */
  unsigned shots=0;uint8_t live=0;
  for(unsigned t=0;t<36;++t) {
    frame(SNES_PAD_Y);
    for(unsigned i=0;i<8;++i) {
      bool on=g_ram[0x1228+i*64]!=0;
      if(on && !(live&(1u<<i))) ++shots;
      if(on) live|=1u<<i; else live&=~(1u<<i);
    }
  }
  fprintf(stderr,"axl rapid fire: %u shots in 36 frames\n",shots);
  check(shots>=5,"held SHOT rapid-fires");
  check(g_ram[0xc00]==0 || g_ram[0xbff]>0x9f,"held SHOT never builds a charge");
  check(MmxAxlFrame(g_ram,&f) && f.sprite_count>0 && f.hidden_slots,"Axl's bullets replace the lemons");

  axl_aim_checks(base,base_n);
  axl_weapon_swap_checks(base,base_n);

  /* Long rapid fire in every aim, on the ground and in the air: it must keep
   * firing, and X must still fire afterwards. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  {
    static const unsigned aims[]={0,SNES_PAD_UP,SNES_PAD_UP|SNES_PAD_RIGHT,SNES_PAD_DOWN|SNES_PAD_RIGHT,
        SNES_PAD_DOWN,SNES_PAD_LEFT,SNES_PAD_UP|SNES_PAD_LEFT,SNES_PAD_DOWN|SNES_PAD_LEFT};
    unsigned late=0;uint8_t seen=axl_live_slots();
    for(unsigned t=0;t<2400;++t) {
      unsigned in=SNES_PAD_Y|aims[(t/90)%8]|((t/45)%4==1?SNES_PAD_B:0);
      frame(in);
      uint8_t now=axl_live_slots();
      if(t>=2160) for(unsigned b=0;b<8;++b) late+=((now&~seen)>>b)&1;
      seen=now;
      if(t%300==0 || (t>=2160 && t%40==0))
        fprintf(stderr,"axl long fire t=%u bdd=%u slots=%02x tracked=%u\n",t,g_ram[0xbdd],now,
            (unsigned)!!MmxAxlGetState().shots[0].slot+!!MmxAxlGetState().shots[1].slot+!!MmxAxlGetState().shots[2].slot);
    }
    check(late>=10,"rapid fire keeps firing after 40 seconds");
    zero_replay(60);
    frame(SNES_PAD_SELECT);frame(0);
    uint8_t before=axl_live_slots();
    frame(SNES_PAD_Y);
    check(!MmxAxlActive() && axl_new_slot(before),"X still fires after Axl's long rapid fire");
  }

  /* Ground aim keeps Axl planted. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  unsigned x0=axl_x();
  for(unsigned t=0;t<10;++t) frame(SNES_PAD_Y|SNES_PAD_LEFT|SNES_PAD_UP);
  check(axl_x()==x0 && !(g_ram[0xbb9]&64),"aiming on the ground turns Axl without moving him");

  /* Dodge roll: Down + Dash, invulnerable while it moves. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  x0=axl_x();
  frame(SNES_PAD_DOWN|SNES_PAD_A);
  bool guarded=true;
  for(unsigned t=0;t<14;++t) {frame(SNES_PAD_DOWN);guarded&=g_ram[0xc13]!=0;}
  for(unsigned t=0;t<6;++t) frame(SNES_PAD_DOWN);
  fprintf(stderr,"axl roll: x %u -> %u\n",x0,axl_x());
  check(MmxAxlGetState().roll && guarded,"Down + Dash rolls with invulnerability");
  check(x0-axl_x()>=40 || axl_x()-x0>=40,"the roll travels");
  for(unsigned t=0;t<20;++t) frame(0);
  check(!MmxAxlGetState().roll,"the roll ends");
  /* However the roll's flash and X1's blink line up, Axl is visible after
   * every roll. */
  {
    unsigned hidden=0;
    for(unsigned k=0;k<12;++k) {
      frame(k&1?SNES_PAD_RIGHT:SNES_PAD_LEFT);
      for(unsigned w=0;w<k%4;++w) frame(0);
      frame(SNES_PAD_DOWN|SNES_PAD_A);
      for(unsigned t=0;t<40;++t) frame(0);
      MmxRenderAxlFrame r;
      if(!(MmxAxlFrame(g_ram,&r) && r.body.active && g_ram[0xbb6])) ++hidden;
    }
    fprintf(stderr,"axl rolls left hidden: %u of 12\n",hidden);
    check(!hidden,"Axl is visible after every roll");
  }

  /* Hover: jump again and hold. Level for two seconds, then a slow sink. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  for(unsigned t=0;t<12;++t) frame(SNES_PAD_B);
  frame(0);frame(SNES_PAD_B);
  unsigned hy=axl_y();
  for(unsigned t=0;t<60;++t) frame(SNES_PAD_B);
  fprintf(stderr,"axl hover: y %u -> %u hover=%u\n",hy,axl_y(),MmxAxlGetState().hover);
  check(MmxAxlGetState().hover && axl_y()<=hy+2 && axl_y()+2>=hy,"holding jump hovers in place");
  x0=axl_x();
  for(unsigned t=0;t<20;++t) frame(SNES_PAD_B|SNES_PAD_RIGHT);
  check(axl_x()>x0 && axl_x()<x0+40,"Axl drifts slowly while hovering");
  hy=axl_y();
  for(unsigned t=0;t<60;++t) frame(SNES_PAD_B);
  fprintf(stderr,"axl hover sink: y %u -> %u\n",hy,axl_y());
  check(axl_y()>hy+20 && axl_y()<hy+90,"after the hover time Axl sinks slowly while jump is held");

  /* Air dash in 8 directions: right, and straight up. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  for(unsigned t=0;t<10;++t) frame(SNES_PAD_B);
  x0=axl_x();unsigned y0=axl_y();
  frame(SNES_PAD_A|SNES_PAD_LEFT);
  for(unsigned t=0;t<20;++t) frame(0);
  fprintf(stderr,"axl air dash left: x %u -> %u y %u -> %u\n",x0,axl_x(),y0,axl_y());
  check(x0-axl_x()>=50 && axl_y()<=y0+3,"the air dash flies level");
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  for(unsigned t=0;t<6;++t) frame(SNES_PAD_B);
  x0=axl_x();y0=axl_y();
  frame(SNES_PAD_A|SNES_PAD_UP|SNES_PAD_LEFT);
  for(unsigned t=0;t<12;++t) frame(0);
  fprintf(stderr,"axl air dash up-left: x %u -> %u y %u -> %u\n",x0,axl_x(),y0,axl_y());
  /* The corridor ceiling is at y=626, a few pixels above this dash. */
  check(x0-axl_x()>=20 && y0-axl_y()>=5 && axl_y()<=627,"the air dash flies diagonally up");
  /* From the ground: up-left, up and up-right all take off. */
  {
    static const unsigned ups[3]={SNES_PAD_UP|SNES_PAD_LEFT,SNES_PAD_UP,SNES_PAD_UP|SNES_PAD_RIGHT};
    for(unsigned k=0;k<3;++k) {
      check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
      x0=axl_x();y0=axl_y();
      frame(SNES_PAD_A|ups[k]);
      unsigned top=y0;
      for(unsigned t=0;t<14;++t) {frame(ups[k]);if(axl_y()<top) top=axl_y();}
      fprintf(stderr,"axl ground dash up %u: x %u -> %u y %u -> top %u dash=%u act=%02x\n",k,x0,axl_x(),y0,top,
          MmxAxlGetState().dash,g_ram[0xbaa]);
      check(y0-top>=20,"Axl dashes upward from the ground");
    }
  }

  /* Shot2 with no weapon: the copy shot. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  check(!g_ram[0xbdb],"the corridor starts on the buster");
  uint8_t before=axl_live_slots();
  frame(SNES_PAD_X);
  unsigned slot=axl_new_slot(before);
  bool copy=false;
  for(unsigned i=0;i<MMX_AXL_SHOTS;++i) copy|=MmxAxlGetState().shots[i].slot==slot && MmxAxlGetState().shots[i].kind==1;
  check(slot && copy,"SHOT2 fires the copy shot with no weapon");

  /* A shot's pose keeps its aim until it ends, also after the button is
   * let go; the pistol bullet turns along its aim. */
  {
    static const unsigned pads[2]={SNES_PAD_Y,SNES_PAD_X};
    for(unsigned k=0;k<2;++k) {
      check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
      frame(pads[k]|SNES_PAD_UP);
      bool held=true;unsigned posed=0;
      for(unsigned t=0;t<20;++t) {
        frame(0);MmxAxlState s=MmxAxlGetState();
        if(s.anim==MMX_AXL_ANIM_SHOT) {++posed;held&=s.anim_step==0;}
      }
      fprintf(stderr,"axl shot pose %u: posed=%u held=%u\n",k,posed,held);
      check(posed>=10 && held,"the shot pose stays aimed up until it ends");
    }
    check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
    frame(SNES_PAD_Y|SNES_PAD_UP);frame(0);
    MmxRenderAxlFrame up;MmxAxlFrame(g_ram,&up);
    check(up.sprite_count>0 && up.sprites[0].plane.height>up.sprites[0].plane.width,"a bullet fired up flies upright");
  }

  /* The roll's blank flash frames still hide X's body. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  frame(SNES_PAD_DOWN|SNES_PAD_A);
  {
    unsigned blank=0;
    for(unsigned t=0;t<12;++t) {
      frame(SNES_PAD_DOWN);MmxRenderAxlFrame r;
      if(MmxAxlFrame(g_ram,&r) && !r.body.active) blank+=r.own_body;
    }
    check(blank>0,"X never shows through the roll's flash");
  }

  /* A down air dash stops when it lands or when its time is up. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  for(unsigned t=0;t<10;++t) frame(SNES_PAD_B);
  frame(SNES_PAD_A|SNES_PAD_DOWN);
  check(MmxAxlGetState().anim==MMX_AXL_ANIM_DASH_DOWN,"the down dash uses the turned dash art");
  for(unsigned t=0;t<20;++t) frame(0);
  fprintf(stderr,"axl down dash: dash=%u act=%02x ground=%u\n",MmxAxlGetState().dash,g_ram[0xbaa],g_ram[0xbd3]&4);
  check(!MmxAxlGetState().dash && (g_ram[0xbd3]&4),"the down dash ends on landing");

  /* On a wall, shooting holds Axl in place and fires away from it, even
   * with the d-pad held into the wall. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  {
    unsigned wall=0;
    for(unsigned t=0;t<400 && g_ram[0xbaa]!=0x12;++t) frame(SNES_PAD_LEFT|(t%30<20?SNES_PAD_B:0));
    wall=g_ram[0xbaa]==0x12;
    fprintf(stderr,"axl wall: act=%02x x=%u y=%u face=%02x\n",g_ram[0xbaa],axl_x(),axl_y(),g_ram[0xbb9]&64);
    check(wall,"Axl reaches the corridor's left wall");
    unsigned wy=axl_y(),face=g_ram[0xbb9]&64;
    uint8_t seen=axl_live_slots();int vx=0;bool level=true;
    for(unsigned t=0;t<24;++t) {
      frame(SNES_PAD_Y|SNES_PAD_LEFT|(t>=12?SNES_PAD_UP:0));
      uint8_t now=axl_live_slots();
      for(unsigned i=0;i<MMX_AXL_SHOTS;++i) {
        MmxAxlShot s=MmxAxlGetState().shots[i];
        if(s.slot && (now&~seen&(1u<<((s.slot-0x1228)/64)))) {vx|=s.vx>0?1:s.vx<0?2:4;if(t>=14) level&=s.vx==0;}
      }
      seen=now;
    }
    fprintf(stderr,"axl wall shots: y %u -> %u act=%02x vx=%d face %02x -> %02x\n",wy,axl_y(),g_ram[0xbaa],vx,face,g_ram[0xbb9]&64);
    check(g_ram[0xbaa]==0x12 && axl_y()<=wy+1,"shooting on a wall holds Axl there");
    check(!(vx&2) && (vx&1) && (g_ram[0xbb9]&64),"Axl never shoots into the wall");
    check(level,"diagonally into the wall aims straight up");
    /* Diagonally away from the wall: up-away and down-away. */
    for(unsigned k=0;k<2;++k) {
      unsigned vert=k?SNES_PAD_DOWN:SNES_PAD_UP;bool away=false,clung=true;
      seen=axl_live_slots();
      for(unsigned t=0;t<14;++t) {
        frame(SNES_PAD_Y|SNES_PAD_RIGHT|vert);clung&=g_ram[0xbaa]==0x12;
        uint8_t now=axl_live_slots();
        for(unsigned i=0;i<MMX_AXL_SHOTS;++i) {
          MmxAxlShot s=MmxAxlGetState().shots[i];
          if(s.slot && (now&~seen&(1u<<((s.slot-0x1228)/64))))
            away|=s.vx>0 && (k?s.vy>0:s.vy<0);
        }
        seen=now;
      }
      check(away && clung,k?"Axl shoots down and away from the wall":"Axl shoots up and away from the wall");
    }
    /* SHOT or SHOT2 held keeps him on the wall with the d-pad let go or
     * pushed away; letting go of both lets him drop. */
    bool stuck=true;
    for(unsigned t=0;t<30;++t) {frame(SNES_PAD_Y|(t>=15?SNES_PAD_RIGHT:0));stuck&=g_ram[0xbaa]==0x12;}
    for(unsigned t=0;t<10;++t) {frame(SNES_PAD_X);stuck&=g_ram[0xbaa]==0x12;}
    check(stuck,"holding SHOT or SHOT2 keeps Axl on the wall");
    for(unsigned t=0;t<10;++t) frame(0);
    check(g_ram[0xbaa]!=0x12,"letting go drops him from the wall");
  }

  /* The HUD badge is Axl's A, and the pause menu shows his portrait and
   * life head instead of X's. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  {
    MmxRenderAxlFrame f;
    frame(0);
    check(MmxAxlFrame(g_ram,&f) && f.badge.pixels && !f.menu,"Axl's badge on the health bar");
    frame(SNES_PAD_START);
    for(unsigned t=0;t<80;++t) frame(0);
    check(MmxAxlFrame(g_ram,&f) && f.menu && f.portrait.pixels && f.life.pixels,
        "the pause menu shows Axl's portrait and life head");
  }

  /* Shot2 with a weapon: its charged shot. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  frame(SNES_PAD_R);zero_replay(20);
  fprintf(stderr,"axl weapon after R: %u\n",g_ram[0xbdb]);
  check(g_ram[0xbdb]!=0,"R selects a weapon");
  before=axl_live_slots();
  frame(SNES_PAD_X);zero_replay(2);
  slot=axl_new_slot(before);
  copy=false;
  for(unsigned i=0;i<MMX_AXL_SHOTS;++i) copy|=MmxAxlGetState().shots[i].slot && MmxAxlGetState().shots[i].kind==1;
  check(slot && !copy,"SHOT2 fires the weapon's charged shot");
  /* Every special weapon, without the arm parts: the charged class (+9). */
  for(unsigned w=1;w<9;++w) {
    check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
    for(unsigned k=0;k<w;++k) {frame(SNES_PAD_R);zero_replay(12);}
    g_ram[0x1f99]&=(uint8_t)~2;
    before=axl_live_slots();
    frame(SNES_PAD_X);zero_replay(2);
    slot=axl_new_slot(before);
    fprintf(stderr,"axl shot2 weapon %u: class %02x arms %u\n",g_ram[0xbdb],slot?g_ram[slot+10]:0,g_ram[0x1f99]&2);
    check(slot && g_ram[slot+10]>=0x10 && g_ram[slot+10]<=0x17,"SHOT2 fires the full charge without the arm parts");
    check(!(g_ram[0x1f99]&2),"SHOT2 gives the arm parts back");
  }

  /* Armor ability: GigaCrash, one screen-sized hit, invulnerable. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  RtlSetPadExt(0,1);frame(0);RtlSetPadExt(0,0);
  MmxAxlState a=MmxAxlGetState();
  check(a.giga>0 && a.special==0,"ARMOR ABILITY starts GigaCrash and spends the bar");
  bool box=false,still=true;x0=axl_x();
  for(unsigned t=0;t<100;++t) {
    frame(SNES_PAD_RIGHT);
    a=MmxAxlGetState();
    guarded&=g_ram[0xc13]!=0;
    if(a.giga_slot) box|=(g_ram[a.giga_slot+0x20]|g_ram[a.giga_slot+0x21]<<8)==0xfff0;
    still&=axl_x()==x0;
  }
  check(guarded,"GigaCrash makes Axl invulnerable");
  check(box,"GigaCrash holds its screen-sized hitbox");
  check(still,"Axl stands still through GigaCrash");
  RtlSetPadExt(0,1);frame(0);RtlSetPadExt(0,0);
  for(unsigned t=0;t<100;++t) frame(0);
  check(!MmxAxlGetState().giga,"GigaCrash ends and needs a full bar again");
  /* Its box ($86:FFF0) lies in Saber Zero's ground window: gone afterwards. */
  {
    static const uint8_t empty[4]={255,255,255,255};
    check(!memcmp(g_snes->cart->rom+0x37ff0,empty,4),"GigaCrash's box leaves the ROM padding empty");
  }

  /* GigaCrash on the arena's two bosses: each takes one boss-sized hit. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  MmxBossRushState rush=MmxBossRushGetState();
  for(unsigned t=0;t<1500 && MmxBossRushGetState().mode!=MMX_RUSH_PLAYING;++t) {
    frame(SNES_PAD_RIGHT);g_ram[0xbcf]=0xa0;
  }
  for(unsigned t=0;t<2400;++t) {
    frame(0);g_ram[0xbcf]=0xa0;
    rush=MmxBossRushGetState();
    if(rush.bosses[0].phase==MMX_RUSH_FIGHTING && rush.bosses[1].phase==MMX_RUSH_FIGHTING) break;
  }
  check(rush.bosses[0].phase==MMX_RUSH_FIGHTING && rush.bosses[1].phase==MMX_RUSH_FIGHTING,
      "the arena's bosses fight");
  zero_replay(60);
  {
    /* The dodge roll: a boss held on top of Axl hurts him standing, never
     * while he tumbles, and again once he crouches out of it. */
    static uint8_t arena[1<<20];
    size_t arena_n=RtlSaveSnapshotToMemory(arena,sizeof(arena));
    unsigned boss=rush.bosses[0].object;
    for(unsigned pass=0;pass<2;++pass) {
      check(RtlLoadSnapshotFromMemory(arena,arena_n),"arena restores");
      g_ram[0xbcf]=(uint8_t)((g_ram[0xbcf]&128)|20);g_ram[0xc13]=0;g_ram[0xbd8]=0;g_ram[0xc32]=0;
      fprintf(stderr,"axl roll setup boss=%04x id=%02x act=%u xy=%u,%u player=%u,%u bd8=%02x c32=%02x\n",boss,g_ram[boss+10],g_ram[boss],
          g_ram[boss+5]|g_ram[boss+6]<<8,g_ram[boss+8]|g_ram[boss+9]<<8,axl_x(),axl_y(),g_ram[0xbd8],g_ram[0xc32]);
      unsigned hp=20,hurt_at=0;bool hurt=false,flashed=false,shown=false;
      if(pass) frame(SNES_PAD_DOWN|SNES_PAD_A);
      for(unsigned t=0;t<30;++t) {
        memcpy(g_ram+boss+5,g_ram+0xbad,2);memcpy(g_ram+boss+8,g_ram+0xbb0,2);
        unsigned roll=MmxAxlGetState().roll;
        frame(pass?SNES_PAD_DOWN:0);
        if(!hurt && (g_ram[0xbcf]&127)<hp) hurt_at=roll;
        hurt|=(g_ram[0xbcf]&127)<hp;
        MmxRenderAxlFrame f;
        bool drawn=MmxAxlFrame(g_ram,&f) && f.body.active;
        if(pass && MmxAxlGetState().roll && MmxAxlGetState().roll<=16) {flashed|=!drawn;shown|=drawn;}
      }
      fprintf(stderr,"axl roll vs boss pass=%u hurt=%u at roll %u\n",pass,hurt,hurt_at);
      if(!pass) check(hurt,"a boss on top of Axl hurts him");
      else {
        check(hurt && hurt_at>16 && hurt_at<=20,"nothing hurts Axl while he tumbles, but his crouch can be hit");
        check(flashed && shown,"Axl flashes while he tumbles");
        for(unsigned t=0;t<40;++t) frame(0);
        hp=g_ram[0xbcf]&127;hurt=false;
        for(unsigned t=0;t<60;++t) {
          memcpy(g_ram+boss+5,g_ram+0xbad,2);memcpy(g_ram+boss+8,g_ram+0xbb0,2);
          frame(0);hurt|=(g_ram[0xbcf]&127)<hp;
        }
        check(hurt,"after the roll Axl can be hit again");
      }
    }
    /* Death: Axl's own hurt pose, never X's. */
    check(RtlLoadSnapshotFromMemory(arena,arena_n),"arena restores");
    g_ram[0xbcf]=(uint8_t)((g_ram[0xbcf]&128)|1);g_ram[0xc13]=0;g_ram[0xbd8]=0;g_ram[0xc32]=0;
    for(unsigned t=0;t<120 && g_ram[0xbaa]!=0x0c;++t) {
      memcpy(g_ram+boss+5,g_ram+0xbad,2);memcpy(g_ram+boss+8,g_ram+0xbb0,2);frame(0);
    }
    {
      unsigned owned=0,posed=0,frames=0;
      for(unsigned t=0;t<40 && g_ram[0xbaa]==0x0c;++t) {
        frame(0);++frames;MmxRenderAxlFrame f;
        owned+=MmxAxlFrame(g_ram,&f) && f.own_body;
        posed+=f.body.active && MmxAxlGetState().anim==MMX_AXL_ANIM_DAMAGE;
      }
      fprintf(stderr,"axl death: %u frames, owned %u, posed %u\n",frames,owned,posed);
      check(frames && owned==frames && posed,"Axl dies in his own hurt pose");
    }
    check(RtlLoadSnapshotFromMemory(arena,arena_n),"arena restores");
  }
  a=MmxAxlGetState();a.special=MMX_AXL_SPECIAL_MAX*16;MmxAxlSetState(&a);
  unsigned hp0[2];
  for(unsigned i=0;i<2;++i) hp0[i]=g_ram[rush.bosses[i].object+0x27]&127;
  RtlSetPadExt(0,1);frame(0);RtlSetPadExt(0,0);
  unsigned hurt=0;bool hit_axl=false;
  for(unsigned t=0;t<200;++t) {
    unsigned hp=g_ram[0xbcf]&127;
    frame(0);
    hit_axl|=MmxAxlGetState().giga && (g_ram[0xbcf]&127)<hp;
  }
  for(unsigned i=0;i<2;++i) {
    unsigned hp=g_ram[rush.bosses[i].object+0x27]&127;
    fprintf(stderr,"axl giga boss %u: hp %u -> %u\n",i,hp0[i],hp);
    if(hp<hp0[i]) ++hurt;
    check(hp+8>=hp0[i],"GigaCrash hits each boss once");
  }
  check(hurt>=1,"GigaCrash damages the bosses on screen");
  check(!hit_axl,"nothing hurts Axl during GigaCrash");

  /* Save states keep Axl's state (chunk v26). */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  for(unsigned t=0;t<12;++t) frame(SNES_PAD_B|SNES_PAD_Y);
  static uint8_t snap[1<<20];
  size_t n=RtlSaveSnapshotToMemory(snap,sizeof(snap));
  MmxAxlState saved=MmxAxlGetState();
  zero_replay(30);
  check(RtlLoadSnapshotFromMemory(snap,n),"mid-air snapshot restores");
  MmxAxlState loaded=MmxAxlGetState();
  check(!memcmp(&saved,&loaded,sizeof(saved)),"Axl state survives a save state");

  /* CHANGE PLAYER (Select) switches between the team: Axl and X. */
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  frame(SNES_PAD_SELECT);frame(0);
  check(!MmxAxlActive() && !(MmxAxlFrame(g_ram,&f) && f.body.active),"Select switches to X");
  frame(SNES_PAD_SELECT);frame(0);
  check(MmxAxlActive(),"Select switches back to Axl");

  /* Partner None: CHANGE PLAYER does nothing. */
  MmxAxlSetTeam(MMX_AXL_CHAR_AXL,MMX_AXL_CHAR_ALONE);
  check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
  for(unsigned t=0;t<3;++t) {frame(SNES_PAD_SELECT);frame(0);}
  check(MmxAxlActive(),"with no partner Select keeps Axl");
  MmxAxlSetTeam(MMX_AXL_CHAR_AXL,MMX_AXL_CHAR_X);

  /* A team of Axl and Zero (MMX_AXL_ZERO_ASSETS names the X3 Zero cache):
   * Select runs Zero's own exchange both ways and never shows X. */
  const char *zero_assets=getenv("MMX_AXL_ZERO_ASSETS");
  if(zero_assets) {
    check(MmxZeroLoad(zero_assets),"Zero assets load");
    MmxZeroRegisterHooks();
    MmxAxlSetTeam(MMX_AXL_CHAR_AXL,MMX_AXL_CHAR_ZERO);
    check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
    MmxZeroState z=MmxZeroGetState();z.active_x=1;MmxZeroSetState(z);
    MmxAxlState s=MmxAxlGetState();s.active=1;MmxAxlSetState(&s);
    frame(0);
    check(MmxAxlActive(),"the team starts as Axl");
    /* While the beam carries X's body, it carries Axl's art. */
    unsigned beam_x=0,beam_axl=0;
    frame(SNES_PAD_SELECT);
    for(unsigned t=0;t<120 && (MmxZeroSwapping() || !MmxZeroActive());++t) {
      frame(0);MmxRenderAxlFrame r;MmxZeroState zs=MmxZeroGetState();
      if(MmxZeroSwapping() && zs.active_x && MmxZeroSwapPose(&zs)!=255) {
        ++beam_x;beam_axl+=MmxAxlFrame(g_ram,&r) && r.teleport.pixels;
      }
    }
    check(MmxZeroActive() && !MmxAxlActive(),"Select brings in Zero");
    zero_replay(30);
    frame(SNES_PAD_SELECT);
    for(unsigned t=0;t<120 && (MmxZeroSwapping() || MmxZeroActive());++t) {
      frame(0);MmxRenderAxlFrame r;MmxZeroState zs=MmxZeroGetState();
      if(MmxZeroSwapping() && zs.active_x && MmxZeroSwapPose(&zs)!=255) {
        ++beam_x;beam_axl+=MmxAxlFrame(g_ram,&r) && r.teleport.pixels;
      }
    }
    zero_replay(2);
    check(!MmxZeroActive() && MmxAxlActive(),"Select brings Axl back, not X");
    fprintf(stderr,"axl swap beam: %u of %u frames\n",beam_axl,beam_x);
    check(beam_x>0 && beam_axl==beam_x,"CHANGE PLAYER beams Axl, not X");
    /* A team of X and Axl with Zero added: Zero never joins. */
    MmxAxlSetTeam(MMX_AXL_CHAR_X,MMX_AXL_CHAR_AXL);
    check(RtlLoadSnapshotFromMemory(base,base_n),"corridor restores");
    z=MmxZeroGetState();z.active_x=1;MmxZeroSetState(z);
    s=MmxAxlGetState();s.active=0;MmxAxlSetState(&s);
    bool zero_seen=false,axl_seen=false,x_seen=false;
    for(unsigned k=0;k<6;++k) {
      frame(SNES_PAD_SELECT);
      for(unsigned t=0;t<90;++t) {frame(0);zero_seen|=MmxZeroActive() || MmxZeroSwapping();}
      axl_seen|=MmxAxlActive();x_seen|=!MmxAxlActive() && !MmxZeroActive();
    }
    fprintf(stderr,"axl team x+axl: zero=%u axl=%u x=%u\n",zero_seen,axl_seen,x_seen);
    check(!zero_seen && axl_seen && x_seen,"an X and Axl team never brings in Zero");
    MmxAxlSetTeam(MMX_AXL_CHAR_AXL,MMX_AXL_CHAR_X);
  }
  puts("MMX AXL CHECKS PASSED");
}
