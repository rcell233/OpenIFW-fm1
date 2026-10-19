/* SPDX-License-Identifier: GPL-3.0-only */
static void perf_stress_patch(IFWFixedPatch *p) {
    make_preset(p,0);p->fx_bypass=0;
    for(int i=0;i<5;i++)p->fx[i][IFW_FX_Bypass]=0;
    p->v[P_Filter3Enabled]=1;p->v[P_Filter3Stage]=24*65536;p->v[P_Filter3Mix]=5*65536;
    for(int i=0;i<8;i++) {int id=P_Mod1Source1+i*4;p->v[id]=IFW_SRC_LFO+(i&1);p->v[id+1]=IFW_SRC_EG+i%4;p->v[id+2]=IFW_MOD_FM+i%4;p->v[id+3]=65536;}
}
