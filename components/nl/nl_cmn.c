#include "nl.h"



const double gpst0[]={1980,1, 6,0,0,0}; /* gps time reference */
const double gst0 []={1999,8,22,0,0,0}; /* galileo system time reference */
const double bdt0 []={2006,1, 1,0,0,0}; /* beidou time reference */
static double leaps[MAXLEAPS+1][7]={ /* leap seconds (y,m,d,h,m,s,utc-gpst) */
    {2017,1,1,0,0,0,-18},
    {2015,7,1,0,0,0,-17},
    {2012,7,1,0,0,0,-16},
    {2009,1,1,0,0,0,-15},
    {2006,1,1,0,0,0,-14},
    {1999,1,1,0,0,0,-13},
    {1997,7,1,0,0,0,-12},
    {1996,1,1,0,0,0,-11},
    {1994,7,1,0,0,0,-10},
    {1993,7,1,0,0,0, -9},
    {1992,7,1,0,0,0, -8},
    {1991,1,1,0,0,0, -7},
    {1990,1,1,0,0,0, -6},
    {1988,1,1,0,0,0, -5},
    {1985,7,1,0,0,0, -4},
    {1983,7,1,0,0,0, -3},
    {1982,7,1,0,0,0, -2},
    {1981,7,1,0,0,0, -1},
    {0}
};

void lla2enu(ins_t *ins, double *lla, nl_t *enu)
{
    enu[2] = lla[2] - ins->lla0[2];
    enu[1] = (lla[0] - ins->lla0[0]) * ins->Rns;
    enu[0] = (lla[1] - ins->lla0[1]) * ins->Rew;
}

void enu2lla(ins_t *ins, nl_t *enu, double *lla)
{
    lla[2] = enu[2] + ins->lla0[2];
    lla[0] = enu[1] / ins->Rns + ins->lla0[0];
    lla[1] = enu[0] / ins->Rew + ins->lla0[1];
}

double lla2err(double *lla, double *lla0)
{
    double Earth_Re = 6378137;
    double Earth_e = 0.00335281066474748;
    double h0 = lla0[2];
    double lat0 = lla0[0];

    double Rm = Earth_Re * (1 - 2*Earth_e + 3*Earth_e*sin(lat0)*sin(lat0));
    double Rn = Earth_Re * (1 + Earth_e*sin(lat0)*sin(lat0));
    double Rmh = Rm + h0;
    double Rnh = Rn + h0;

    double err_n = (lla[0] - lla0[0]) * (Rmh);
    double err_e = (lla[1] - lla0[1]) * (Rnh) * cos(lat0);
    double pos_err = sqrt(err_e*err_e + err_n*err_n);

    return pos_err;
}

nl_gtime_t nl_epoch2time(const double *ep)
{
    const int doy[]={1,32,60,91,121,152,182,213,244,274,305,335};
    nl_gtime_t time={0};
    int days,sec,year=(int)ep[0],mon=(int)ep[1],day=(int)ep[2];
    
    if (year<1970||2099<year||mon<1||12<mon) return time;
    
    /* leap year if year%4==0 in 1901-2099 */
    days=(year-1970)*365+(year-1969)/4+doy[mon-1]+day-2+(year%4==0&&mon>=3?1:0);
    sec=(int)floor(ep[5]);
    time.time=(time_t)days*86400+(int)ep[3]*3600+(int)ep[4]*60+sec;
    time.sec=ep[5]-sec;
    return time;
}

void nl_deg2dms(double deg, double *dms, int ndec)
{
    double sign=deg<0.0?-1.0:1.0,a=fabs(deg);
    double unit=pow(0.1,ndec);
    dms[0]=floor(a); a=(a-dms[0])*60.0;
    dms[1]=floor(a); a=(a-dms[1])*60.0;
    dms[2]=floor(a/unit+0.5)*unit;
    if (dms[2]>=60.0) {
        dms[2]=0.0;
        dms[1]+=1.0;
        if (dms[1]>=60.0) {
            dms[1]=0.0;
            dms[0]+=1.0;
        }
    }
    dms[0]*=sign;
}

void nl_time2epoch(nl_gtime_t t, nl_t *ep)
{
    const int mday[]={ /* # of days in a month */
        31,28,31,30,31,30,31,31,30,31,30,31,31,28,31,30,31,30,31,31,30,31,30,31,
        31,29,31,30,31,30,31,31,30,31,30,31,31,28,31,30,31,30,31,31,30,31,30,31
    };
    int days,sec,mon,day;
    
    /* leap year if year%4==0 in 1901-2099 */
    days=(int)(t.time/86400);
    sec=(int)(t.time-(time_t)days*86400);
    for (day=days%1461,mon=0;mon<48;mon++) {
        if (day>=mday[mon]) day-=mday[mon]; else break;
    }
    ep[0]=1970+days/1461*4+mon/12; ep[1]=mon%12+1; ep[2]=day+1;
    ep[3]=sec/3600; ep[4]=sec%3600/60; ep[5]=sec%60+t.sec;
}

uint32_t nl_getbitu(const uint8_t *buf, int pos, int len)
{
    uint32_t bits = 0;
    int i;
    for (i=pos;i<pos+len;i++) bits=(bits<<1)+((buf[i/8]>>(7-i%8))&1u);
    return bits;
}

nl_gtime_t nl_gpst2time(int week, double sec)
{
    nl_gtime_t t=nl_epoch2time(gpst0);
    
    if (sec<-1E9||1E9<sec) sec=0.0;
    t.time+=(time_t)86400*7*week+(int)sec;
    t.sec=sec-(int)sec;
    return t;
}

double nl_time2gpst(nl_gtime_t t, int *week)
{
    nl_gtime_t t0=nl_epoch2time(gpst0);
    time_t sec=t.time-t0.time;
    int w=(int)(sec/(86400*7));
    
    if (week) *week=w;
    return (double)(sec-(double)w*86400*7)+t.sec;
}

nl_gtime_t nl_utc2gpst(nl_gtime_t t)
{
    int i;
    
    for (i=0;leaps[i][0]>0;i++) 
    {
        if (nl_timediff(t,nl_epoch2time(leaps[i]))>=0.0) return nl_timeadd(t,-leaps[i][6]);
    }
    return t;
}

double nl_timediff(nl_gtime_t t1, nl_gtime_t t2)
{
    return t1.time - t2.time + t1.sec-t2.sec;
}

nl_gtime_t nl_timeadd(nl_gtime_t t, double sec)
{
    double tt;
    
    t.sec+=sec; tt=floor(t.sec); t.time+=(int)tt; t.sec-=tt;
    return t;
}


nl_gtime_t nl_gpst2utc(nl_gtime_t t)
{
    nl_gtime_t tu;
    int i;
    
    for (i=0;leaps[i][0]>0;i++) {
        tu=nl_timeadd(t,leaps[i][6]);
        if (nl_timediff(tu, nl_epoch2time(leaps[i]))>=0.0) return tu;
    }
    return t;
}

///* satellite system+prn/slot number to satellite number ------------------------
//* convert satellite system+prn/slot number to satellite number
//* args   : int    sys       I   satellite system (SYS_GPS,SYS_GLO,...)
//*          int    prn       I   satellite prn/slot number
//* return : satellite number (0:error)
//*-----------------------------------------------------------------------------*/
//int nl_satno(int sys, int prn)
//{
//    if (prn<=0) return 0;
//    switch (sys) {
//        case SYS_GPS:
//            if (prn<MINPRNGPS||MAXPRNGPS<prn) return 0;
//            return prn-MINPRNGPS+1;
//        case SYS_GLO:
//            if (prn<MINPRNGLO||MAXPRNGLO<prn) return 0;
//            return NSATGPS+prn-MINPRNGLO+1;
//        case SYS_GAL:
//            if (prn<MINPRNGAL||MAXPRNGAL<prn) return 0;
//            return NSATGPS+NSATGLO+prn-MINPRNGAL+1;
//        case SYS_QZS:
//            if (prn<MINPRNQZS||MAXPRNQZS<prn) return 0;
//            return NSATGPS+NSATGLO+NSATGAL+prn-MINPRNQZS+1;
//        case SYS_CMP:
//            if (prn<MINPRNCMP||MAXPRNCMP<prn) return 0;
//            return NSATGPS+NSATGLO+NSATGAL+NSATQZS+prn-MINPRNCMP+1;
//        case SYS_LEO:
//            if (prn<MINPRNLEO||MAXPRNLEO<prn) return 0;
//            return NSATGPS+NSATGLO+NSATGAL+NSATQZS+NSATCMP+NSATIRN+
//                   prn-MINPRNLEO+1;
//        case SYS_SBS:
//            if (prn<MINPRNSBS||MAXPRNSBS<prn) return 0;
//            return NSATGPS+NSATGLO+NSATGAL+NSATQZS+NSATCMP+NSATIRN+NSATLEO+
//                   prn-MINPRNSBS+1;
//    }
//    return 0;
//}


//int nl_satsys(int sat, int *prn)
//{
//    int sys=SYS_NONE;
//    if (sat<=0||MAXSAT<sat) sat=0;
//    else if (sat<=NSATGPS) {
//        sys=SYS_GPS; sat+=MINPRNGPS-1;
//    }
//    else if ((sat-=NSATGPS)<=NSATGLO) {
//        sys=SYS_GLO; sat+=MINPRNGLO-1;
//    }
//    else if ((sat-=NSATGLO)<=NSATGAL) {
//        sys=SYS_GAL; sat+=MINPRNGAL-1;
//    }
//    else if ((sat-=NSATGAL)<=NSATQZS) {
//        sys=SYS_QZS; sat+=MINPRNQZS-1; 
//    }
//    else if ((sat-=NSATQZS)<=NSATCMP) {
//        sys=SYS_CMP; sat+=MINPRNCMP-1; 
//    }
//    else if ((sat-=NSATCMP)<=NSATIRN) {
//        sys=SYS_IRN; sat+=MINPRNIRN-1; 
//    }
//    else if ((sat-=NSATIRN)<=NSATLEO) {
//        sys=SYS_LEO; sat+=MINPRNLEO-1; 
//    }
//    else if ((sat-=NSATLEO)<=NSATSBS) {
//        sys=SYS_SBS; sat+=MINPRNSBS-1; 
//    }
//    else sat=0;
//    if (prn) *prn=sat;
//    return sys;
//}

/* satellite id to satellite number --------------------------------------------
* convert satellite id to satellite number
* args   : char   *id       I   satellite id (nn,Gnn,Rnn,Enn,Jnn,Cnn,Inn or Snn)
* return : satellite number (0: error)
* notes  : 120-142 and 193-199 are also recognized as sbas and qzss
*-----------------------------------------------------------------------------*/
//int nl_satid2no(const char *id)
//{
//    int sys,prn;
//    char code;
//    
//    if (sscanf(id,"%d",&prn)==1) {
//        if      (MINPRNGPS<=prn&&prn<=MAXPRNGPS) sys=SYS_GPS;
//        else if (MINPRNSBS<=prn&&prn<=MAXPRNSBS) sys=SYS_SBS;
//        else if (MINPRNQZS<=prn&&prn<=MAXPRNQZS) sys=SYS_QZS;
//        else return 0;
//        return nl_satno(sys,prn);
//    }
//    if (sscanf(id,"%c%d",&code,&prn)<2) return 0;
//    
//    switch (code) {
//        case 'G': sys=SYS_GPS; prn+=MINPRNGPS-1; break;
//        case 'R': sys=SYS_GLO; prn+=MINPRNGLO-1; break;
//        case 'E': sys=SYS_GAL; prn+=MINPRNGAL-1; break;
//        case 'J': sys=SYS_QZS; prn+=MINPRNQZS-1; break;
//        case 'C': sys=SYS_CMP; prn+=MINPRNCMP-1; break;
//        case 'L': sys=SYS_LEO; prn+=MINPRNLEO-1; break;
//        case 'S': sys=SYS_SBS; prn+=100; break;
//        default: return 0;
//    }
//    return nl_satno(sys,prn);
//}

/* satellite number to satellite id --------------------------------------------
* convert satellite number to satellite id
* args   : int    sat       I   satellite number
*          char   *id       O   satellite id (Gnn,Rnn,Enn,Jnn,Cnn,Inn or nnn)
* return : none
*-----------------------------------------------------------------------------*/
//void nl_satno2id(int sat, char *id)
//{
//    int prn;
//    switch (nl_satsys(sat,&prn)) {
//        case SYS_GPS: sprintf(id,"G%02d",prn-MINPRNGPS+1); return;
//        case SYS_GLO: sprintf(id,"R%02d",prn-MINPRNGLO+1); return;
//        case SYS_GAL: sprintf(id,"E%02d",prn-MINPRNGAL+1); return;
//        case SYS_QZS: sprintf(id,"J%02d",prn-MINPRNQZS+1); return;
//        case SYS_CMP: sprintf(id,"C%02d",prn-MINPRNCMP+1); return;
//        case SYS_LEO: sprintf(id,"L%02d",prn-MINPRNLEO+1); return;
//        case SYS_SBS: sprintf(id,"%03d" ,prn); return;
//    }
//    strcpy(id,"");
//}

/* 
    calculate baseline heading, pitch, length, vector
*/
//int baseline_hplv(double *rb, double *rr, double *enu, double *heading, double *pitch, double *length, double *normlz_enu)
//{
//    int i;
//    double baseline_ecef[3], pos[3];
//    for (i = 0; i < 3; i++)
//    baseline_ecef[i] = rr[i] - rb[i]; /* rb(base): antB/base/rtklib:instance1, rr(user): user/antA/rtklib:instance0 */
//    ecef2pos(rb, pos);
//    ecef2enu(pos, baseline_ecef, enu);

//    nl_vec2hpl(enu, heading, pitch, length);

//    normlz_enu[0] = enu[0] / *length;
//    normlz_enu[1] = enu[1] / *length;
//    normlz_enu[2] = enu[2] / *length;
//}

/* convert heading/pitch/len to enu[3]
  [I]: heading: rad
  [I]: pitch: rad
  [I]: len: m
  [O]: enu: enu vector
 */
void nl_hpl2vec(nl_t heading, nl_t pitch, nl_t len, nl_t *enu)
{
  enu[0] = sin(heading) * cos(pitch) * len;
  enu[1] = cos(heading) * cos(pitch) * len;
  enu[2] = sin(pitch) * len;
}

/* convert enu[3] to heading/pitch/len
  [I]: enu: enu vector
  [O]: heading: rad, -pi:pi, 
  [O]: pitch: rad
  [O]: len: m
 */
void nl_vec2hpl(nl_t *enu, nl_t *heading, nl_t *pitch, nl_t *len)
{
    *len = v3norm(enu);
    *heading = atan2(enu[0], enu[1]);

    *pitch = asin(enu[2] / *len);
}

/* convert 0-2pi clockwise -> -pi:pi auti-clockwise */
nl_t yaw_convert_2pi_to_npi_pi(nl_t angle)
{
    if(angle > PI) angle -= 2*PI;
    return -angle;
}

/* convert -pi:pi auti-clockwise -> 0-2pi clockwise */
nl_t yaw_convert_npi_pi_to_2pi(nl_t angle)
{
    if(angle > 0 && angle < PI) angle = 2*PI - angle;
    if(angle < 0 ) angle = -angle;
    return angle;
}


/* transform ecef to geodetic postion ------------------------------------------
 * transform ecef position to geodetic position
 * args   : double *r        I   ecef position {x,y,z} (m)
 *          double *lla      O   geodetic position {lat,lon,h} (rad,m)
 * return : none
 * notes  : WGS84, ellipsoidal height
 *-----------------------------------------------------------------------------*/
void ecef2lla(const double *r, double *lla)
{
    
    double e2 = FE_WGS84 * (2.0 - FE_WGS84), r2 = POW2(r[0]) + POW2(r[1]), z, zk, v = RE_WGS84, sinp;

    for (z = r[2], zk = 0.0; fabs(z - zk) >= 1E-4;)
    {
        zk = z;
        sinp = z / sqrt(r2 + z * z);
        v = RE_WGS84 / sqrt(1.0 - e2 * sinp * sinp);
        z = r[2] + v * e2 * sinp;
    }
    lla[0] = r2 > 1E-12 ? atan(z / sqrt(r2)) : (r[2] > 0.0 ? PI / 2.0 : -PI / 2.0);
    lla[1] = r2 > 1E-12 ? atan2(r[1], r[0]) : 0.0;
    lla[2] = sqrt(r2 + z * z) - v;
}

/* transform geodetic to ecef position -----------------------------------------
 * transform geodetic position to ecef position
 * args   : double *lla      I   geodetic position {lat,lon,h} (rad,m)
 *          double *r        O   ecef position {x,y,z} (m)
 * return : none
 * notes  : WGS84, ellipsoidal height
 *-----------------------------------------------------------------------------*/
void lla2ecef(const double *lla, double *r)
{
    double sinp = sin(lla[0]), cosp = cos(lla[0]), sinl = sin(lla[1]), cosl = cos(lla[1]);
    double e2 = FE_WGS84 * (2.0 - FE_WGS84), v = RE_WGS84 / sqrt(1.0 - e2 * sinp * sinp);

    r[0] = (v + lla[2]) * cosp * cosl;
    r[1] = (v + lla[2]) * cosp * sinl;
    r[2] = (v * (1.0 - e2) + lla[2]) * sinp;
}

/* simple algorithm to compute ENU between 2 LLA(rad)
    see Gavous: (2. 111), GPS XieGang, 3.21A-3.21C
*/
nl_t lla2enu2(double lat0, double lon0, double hgt0, double lat1, double lon1, double hgt1)
{
    nl_t delta_e = RE_WGS84 * (lon1 - lon0) * cos(lat0);
    nl_t delta_n = RE_WGS84 * (lat1 - lat0);
    nl_t delta_u = hgt1 - hgt0;

    return sqrt(POW2(delta_e) + POW2(delta_n) + POW2(delta_u));
}



