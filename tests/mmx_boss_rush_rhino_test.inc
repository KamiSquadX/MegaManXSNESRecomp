/* Tunnel Rhino's original X3 code, relocated from the user's ROM, fights in
 * seat 1 beside a native Penguin. MMX_BOSS_RUSH_X3_ROM names the X3 ROM. */
static void rush_rhino_checks(uint8_t *start,size_t cap,const char *x3) {
  char error[256];
  check(x3 && MmxBossRushForeignLoad(3,x3,error,sizeof(error)),"X3 ROM loads for the Rhino fixture");
  check(MmxBossRushForeignBuild(error,sizeof(error)),"Rhino relocates into the cart padding");
  if(error[0]) fprintf(stderr,"rhino build: %s\n",error);
  check(MmxBossRushForeignController(23)==0xbfe5e4 && MmxBossRushForeignChild(23,0x26)==0xb8ab9b,
      "Rhino body and drill controllers register");
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
  for(unsigned t=0;t<1800;++t) {
    frame(0);rush_keep_players_alive();s=MmxBossRushGetState();
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
    for(unsigned d=0x1428;d<0x1628;d+=64)
      if(g_ram[d] && g_ram[d+10]==0x26 && MmxBossRushOwner(d)==1) ++drill_frames;
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
  size_t n=RtlSaveSnapshotToMemory(start,cap);check(n,"Rhino seat snapshot saves");
  static uint8_t first[0x20000];
  for(unsigned t=0;t<60;++t) {frame(0);rush_keep_players_alive();}
  memcpy(first,g_ram,sizeof(first));
  check(RtlLoadSnapshotFromMemory(start,n),"Rhino seat snapshot restores");
  for(unsigned t=0;t<60;++t) {frame(0);rush_keep_players_alive();}
  check(!memcmp(first,g_ram,sizeof(first)),"Rhino replay is deterministic");
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
