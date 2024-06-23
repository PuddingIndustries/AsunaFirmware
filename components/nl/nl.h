#ifndef _NL_H
#define _NL_H

#ifdef __cplusplus
extern "C"{
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

///* just for currert development stage */
//#include "rtklib.h"

#include "nl_cfg.h"
#include "nl_def.h"
#include "nl_debug.h"


typedef struct
{
    int (*get_raw)(nl_t *acc, nl_t *gyr, uint32_t size);
    int (*get_raw_mag)(nl_t *mag, uint32_t size);
    int (*get_raw_temperature)(nl_t *temp);
}nl_imu_ops_t;

/* 
    those param is determinted during the factory calibration process
*/
typedef struct
{
 nl_t            gmis_data[9];       /* acc mis 3x3 */
 nl_t            wmis_data[9];       /* gyr mis 3x3 */
 nl_t            mmis_data[9];       /* mag mis 3x3 */
 nl_t            urfr_data[9];       /* URFR mis, user reference rotation */
 nl_t            gb[3];              /* acc bias */
 nl_t            wb[3];              /* gyr bias */
 nl_t            mb[3];              /* mag bias(hard iron) */
 nl_t            gb_tc[3];         /* 1st order temperature compensation, for acc bias， f(x) = gb_tc*x + gb, .C, f(x) = m/s^(2), where x = (temperature: deg) */
 nl_t            wb_tc[3];         /* 1st order temperature compensation, for gyr bias， f(x) = wb_tc*x + wb, .C, f(x) = rad/s,   where x = (temperature: deg) */
 nl_t            ws_tc[3];         /* 1st order temperature compensation, for gyr scale factor，*/
 nl_t            cal_temp;
}nl_imu_calib_t;

typedef struct          /* time struct */
{
    time_t time;        /* time (s) expressed by standard time_t */
    double sec;         /* fraction of second under 1 s */
} nl_gtime_t;

typedef struct
{
    nl_t sum;
    nl_t sum_sqrt;
    nl_t avg;
    nl_t std;
    uint32_t count;
}nl_var_calc_t;

typedef struct
{
    nl_t sum_x;
    nl_t sum_y;
    nl_t sum_xy;
    nl_t sum_x2;
    nl_t p0;            /* slope */
    nl_t p1;            /* intercept */
    uint32_t n;
}linreg_t;

typedef struct
{
    nl_imu_ops_t *ops;

    nl_t acc_raw[3];     /* acc raw reading , m/s^(2) */
    nl_t gyr_raw[3];     /* gyr raw reading , rad/s */
    nl_t mag_raw[3];     /* gyr raw reading , rad/s */
    nl_t acc_cal[3];     /* acc reading , factory calbiratied, m/s^(2) */
    nl_t gyr_cal[3];     /* gyr reading , factory calbiratied, rad/s */
    nl_t mag_cal[3];     /* mag reading , factory calbiratied, uT(1 uT = 10 mGauss ，地磁场的范围：250 - 650 mGauss 或 25 - 64 uT) */

    nl_t last_zaru_wb[3];
    nl_t zaru_avg[3];
    /* calibratrion data */
    m_t             gmis;               /* acc mis-aligne mat */
    m_t             wmis;               /* gyr mis-aligne mat */
    m_t             mmis;               /* mag mis-aligne mat */
    m_t             urfr; 

    nl_imu_calib_t   calib;

    /* statis */
    nl_var_calc_t vc_gyr[3];
    nl_var_calc_t vc_acc;
    uint16_t      imu_read_frq;
    uint16_t      mag_read_div;
    uint16_t      mag_read_ctr;
    uint16_t      temp_read_div;
    uint16_t      temp_read_ctr;
    nl_t          nacc;         /* Norm of acc, unit: see acc_raw */
    nl_t          ngyr;         /* Norm of gyr, unit: see gyr_raw */
    nl_t          acc_nstd;     /* Norm of acc std */
    nl_t          temperature;  /* °C */
    nl_t          zaru_thr;
    uint32_t      zaru_time_ms;
    uint8_t       zaru_flag;    /* 0: not valid, 1:valid, 2: valid and it's first time */
    uint16_t      zaru_succ_cnt;
    uint8_t       en_zaru;
    uint8_t       en_tc;
} nl_imu_t;






//typedef struct
//{
//    obs_t obs[3]; /* Rover Master:0, Rover Slave:1, Base:2 */
//    nav_t nav;
//} gnss_raw_t;

//typedef struct
//{
//    nl_t p[3]; /* Position(m) in ecef */
//    nl_t v[3]; /* Velocity(m/s) in ecef */
//} gnss_sat_pv_t;

//typedef struct
//{
//    nl_t bias;  /* Satellite clock bias (s) */
//    nl_t drift; /* Satellite clock drift (s/s) */
//} gnss_sat_dts_t;

//typedef struct
//{
//    gnss_sat_pv_t pv[MAX_OBS_NS];   /* Satellite position and velocity */
//    gnss_sat_dts_t dts[MAX_OBS_NS]; /* Satellite clock bias and drift */
//    nl_t var[MAX_OBS_NS];           /* Satellite position and clock error variance (m^2) */
//    int svh[MAX_OBS_NS];            /* Satellite health flag (-1:correction not available) */
//    int eph[MAX_OBS_NS];            /* Satellite ephemeris flag (-1:correction not available) */
//    int spp_idx[MAX_OBS_NS];        /* Satellite index for spp */
//    int rtk_idx[MAX_OBS_NS];        /* Satellite index for rtk */

//    nl_t e[MAX_OBS_NS][3];                   /* Direction vector in ecef */
//    nl_t dist[MAX_OBS_NS];                   /* Distance(m) */
//    nl_t az[MAX_OBS_NS];                     /* Azimuth(rad) */
//    nl_t el[MAX_OBS_NS];                     /* Elevation(rad) */
//    nl_t lambda[MAX_OBS_NS][GNSS_FREQ_NUM];  /* Wavelength(m) */
//    nl_t freq[MAX_OBS_NS][GNSS_FREQ_NUM];    /* Frequency(Hz) */

//    size_t obs_n; /* Number of observations */
//    size_t spp_n; /* Number of satellites for spp */
//    size_t rtk_n; /* Number of satellites for rtk */
//} gnss_sat_t;

typedef struct /* RTCM control struct type */
{        
    int nbyte;          /* number of bytes in message buffer */ 
    int len;            /* message length (bytes) */
    uint8_t buf[1200];  /* message buffer */
    uint16_t type;
} nl_rtcm_t;


typedef struct
{
    nl_gtime_t gpst;
    uint8_t nv;
    uint8_t nv_heading;
    uint8_t solq;          /* aligned with RTKLIB */
    uint8_t solq_heading;  /* dual_heading solq, alighed with RTKLIB, see: SOLQ_SINGLE */
    double lla[3];         /* lat(rad), lon(rad), m(eplispoid hight(HAE, h_ref) = msl + undulation)： 椭球高=海拔高+undulation */
    nl_t pos_enu_std[3];
    nl_t vel_enu_std[3];
    nl_t vel_enu[3];     /* m/s */
//    nl_t pos_ecef[3];    /* m */
//    nl_t vel_ecef[3];    /* m/s */

    nl_t dual_enu[3];      /* gnss dual baseline enu, m */
    nl_t cog; /* RMC: course over ground */
    nl_t sog; /* RMC: speed over ground */
    nl_t hdop;
    nl_t vdop;
    nl_t pdop;
    nl_t gdop;
    nl_t tdop;
    
    nl_t undulation; /* The difference between ellipsoid height and mean sea level. Equals (h_ref - msl) */
    nl_t diff_age;
    uint8_t leap_sec;
} gnss_sol_t;



typedef struct
{
    /* Initial diagonal elements of matrix P */
    nl_t P0_att[2];       /* attitude [0]=(pitch, roll) [1]=yaw,rad */
    nl_t P0_vel[2];       /* velocity [0]=(X, Y) [1]=Z,m/s */
    nl_t P0_pos[2];       /* position [0]=(X, Y) [1]=Z,m */
    nl_t P0_gyr_bias[2];     /* gyr bias [0]=(X, Y) [1]=Z,rad/s */
    nl_t P0_acc_bias[2];     /* acc bias [0]=(X, Y) [1]=Z,m/s^(2) */

    /* Max restriction diagonal elements of matrix P */
    nl_t Pmax_att[2];     /* follow P0 */
    nl_t Pmax_vel[2];
    nl_t Pmax_pos[2];
    nl_t Pmax_gyr_bias;
    nl_t Pmax_acc_bias;

    /* Min restriction elements of matrix P */
    nl_t Pmin_att[2];    /* follow P0 */
    nl_t Pmin_vel[2];
    nl_t Pmin_pos[2];
    nl_t Pmin_gyr_bias;
    nl_t Pmin_acc_bias;


    /* Process noise */
    nl_t Qgyr_wb;       /* Gyroscope white noise          rad/s   */
    nl_t Qacc_wb;       /* Accelerometer white noise      m/s^(2) */
    nl_t Qgyr_wbb;      /* Gyroscope bias white noise     rad/s   */
    nl_t Qacc_wbb;      /* Accelerometer bias white noise m/s^(2) */

    /* Measurement noise */
    nl_t R0_gnss_vel[2]; /* GNSS's horizontal velocity, vertical velocity, m/s*/
    nl_t R0_gnss_pos[2]; /* GNSS's horizontal position, vertical position, m  */

    nl_t R0_dualgnss;    /* Dual GNSS */

    nl_t R0_zupt;        /* ZUPT */
    nl_t R0_nhc;         /* NHC */
    nl_t R0_gravity;      /* Garivy */
    nl_t R0_od;          /* 里程计 */
   // nl_t R0_zihr;        /* HIZR, 静止Yaw防漂移,  Zero Integrated Heading Rate，ZIHR, 等红绿灯时，yaw会漂移，类似ZUPT的方法，构造虚拟观测量，减小漂移。 see: 1005-6734(2020)06-0701-08： 基于手机内置传感器的车辆组合定位方法, and https://zhuanlan.zhihu.com/p/115529319 */
    nl_t gnss_delay;     /* 时间不同步补偿: s: 最近的IMU采样时间  - 最近的GPS采样时间 */
} eskf156_opt_t;

typedef struct
{
    /* Initial diagonal elements of matrix P */
    nl_t P0_att[2];             /* attitude [0]=(pitch, roll) [1]=yaw,rad */
    nl_t P0_gyr_bias[2];        /* gyr bias [0]=(X, Y) [1]=Z,rad/s */
    nl_t Pmax_att[2];           /* follow P0 */
    nl_t Pmin_att[2];           /* follow P0 */
    nl_t Pmax_gyr_bias;
    nl_t Pmin_gyr_bias;
    
    /* Process noise */
    nl_t Qgyr_wb[2];            /* Gyroscope white noise          rad/s   */
    nl_t Qgyr_wbb[2];           /* Gyroscope bias white noise     rad/s   */
    
    /* Measurement noise */
    nl_t R0_gravity;            /* R0 Garivy */
    nl_t R0_mag;                /* R0 mag */
    nl_t R0_zaru;
} eskfatt_t;


uint32_t nl_getbitu(const uint8_t *buf, int pos, int len);


nl_t *vcreate(size_t num);
void vprint(nl_t *A, size_t n, size_t q);
void vcopy(nl_t *A, nl_t *B, size_t len);
void vfill(nl_t *A, nl_t val, size_t len);
void vadd(nl_t *C, nl_t *A, nl_t *B, size_t len);
void vadd2(nl_t *A, nl_t *B, size_t len);
void vsub(nl_t *C, nl_t *A, nl_t *B, size_t len);
void vsub2(nl_t *A, nl_t *B, size_t len);
void vscale(nl_t *A, nl_t *B, nl_t k, size_t len);
void vscale2(nl_t *A, nl_t k, size_t len);
void vnormlz(nl_t *A, size_t len);
nl_t vnorm(nl_t *A, size_t len);
nl_t mean(nl_t *V, size_t len);
nl_t vdot(nl_t *A, nl_t *B, size_t len);
void vconstrain(nl_t *X, nl_t max, nl_t min, size_t len);
void v3constrain(nl_t *X, nl_t max, nl_t min);

uint32_t vec2str(char *buf, uint32_t size, nl_t *A, size_t n, size_t q);
uint32_t vec2str_scale(char *buf, uint32_t size, nl_t *A, nl_t k, size_t n, size_t q);
uint32_t mat2str(char *buf, uint32_t size, m_t *A, size_t q);
void nl_time2str(nl_gtime_t t, char *s, int n);
void v3print(nl_t *A, size_t q);
void v3print_scale(nl_t A[], size_t q, nl_t scale);
void v3copy(nl_t *A, nl_t *B);
void v3fill(nl_t *A, nl_t val);
void v3add(nl_t *C, nl_t *A, nl_t *B);
void v3add2(nl_t *A, nl_t *B);
void v3sub(nl_t *C, nl_t *A, nl_t *B);
void v3sub2(nl_t *A, nl_t *B);
void v3scale(nl_t *A, nl_t *B, nl_t k);
void v3scale2(nl_t *A, nl_t k);
void v3cross(nl_t *A, nl_t *B, nl_t *C);
void v3normlz(nl_t *A);
nl_t v3norm(nl_t *A);
nl_t v3dot(nl_t *A, nl_t *B);

nl_t var(nl_t *V, size_t len);
nl_t nl_std(nl_t *V, size_t len);

m_t *mcreate(size_t r, size_t c);
m_t *mcreate2(size_t r, size_t c, nl_t *p);
void meye(m_t *A);
void minit(m_t *A, size_t r, size_t c, nl_t *p);
void mprint(m_t *A, size_t q);
void nl_skew(m_t *m, nl_t *v);
void mfill(m_t *A, nl_t val);
void mfilldiag(m_t *A, nl_t val);
void msetdiag(m_t *A, nl_t *V);
void mcopy(m_t *A, m_t *B);
void mbcopy(m_t *A, m_t *m, size_t start_r, size_t start_c);
void madd(m_t *A, m_t *B, m_t *C);
void madd2(m_t *A, m_t *B);
void msub(m_t *A, m_t *B, m_t *C);
void msub2(m_t *A, m_t *B);
void mscale(m_t *A, m_t *B, nl_t k);
void mscale2(m_t *A, nl_t k);
void msetrow(m_t *A, int r, nl_t* V);
void mgetrow(m_t *A, int r, nl_t* V);
void mgetcol(m_t *A, int c, nl_t *V);
void msetcol(m_t *A, int c, nl_t* V);
void madddiag(m_t *A, nl_t *d);
void madddiag2(m_t *A, nl_t alpha, nl_t *d);
void maddeye(m_t *A);
void mvmul(m_t *A, nl_t *v, nl_t *s);
void mmul(m_t *A, m_t *B, m_t *C);
void mmul3(const char *tr, nl_t alpha, m_t *matA, m_t *matB, nl_t beta, m_t *matC);
nl_t mtrace(m_t *A);
void mtrans(m_t *A, m_t *AT);
int minv(m_t *A);
nl_t det(m_t *A);

/* nl_mat2 */
void jcbj(m_t *A, m_t *V, nl_t eps);


nl_t *nl_mat(int n, int m);
int *nl_imat(int n, int m);
void nl_matcpy(nl_t *A, const nl_t *B, int n, int m);

void qmul(nl_t *q1, nl_t *q2, nl_t *r);
void qmul2(nl_t *q1, nl_t *q2);
void qmul3(nl_t *q1, nl_t *q2);
void qnormlz(nl_t *q);
void qidentity(nl_t *q);
void qconj(nl_t *q, nl_t *qj);
void qconj2(nl_t *q);
void qmulv(nl_t *q, nl_t *v, nl_t *r);
void rv2q(nl_t *rv, nl_t *q);
void q2att(nl_t *q, att_t *att);
void att2q(att_t *att, nl_t *q);
void q2dcm(nl_t *q, nl_t *p);

int nl_lsq(m_t *A, nl_t *y, nl_t *x, m_t *Q);
int nl_lsq2(m_t *A, m_t *Y, m_t *X, m_t *Q);

linreg_t *linreg_create(void);
void linreg_clear(linreg_t *lr);
void linreg_fit(linreg_t *lr);
void linreg_add(linreg_t *lr, nl_t x, nl_t y);


void ins_init(ins_t *ins);
void ins_align(ins_t *ins, nl_t *acc, nl_t yaw);
void ins_set_yaw(ins_t *ins, nl_t yaw);
void ins_set_inital_lla(ins_t *ins, double *lla);
void ins_update(ins_t *ins, nl_t *gyro, nl_t *acc, nl_t od_speed, nl_t dt);
int ins2str(char *buf, ins_t *ins);

void kf_state_create(kf_state_t *kf, size_t n);
void kf_meas_create(kf_meas_t *kf, size_t n, size_t m);
void kf_state_update(kf_state_t *s, nl_t dt);
void kf_meas_update(kf_state_t *s, kf_meas_t *m);
void kf_state_free(kf_state_t *s);
void kf_meas_free(kf_meas_t *m);
void eskf156_create(kf_state_t *s, kf_meas_t *gnss_pos, kf_meas_t *gnss_vel, kf_meas_t *dualgnss, kf_meas_t *zupt, kf_meas_t *nhc, kf_meas_t *gravity, kf_meas_t *od, const eskf156_opt_t *opt);
void eskf_reset_P(kf_state_t *s);
void eskf156_state_model(kf_state_t *s, ins_t *ins, nl_t dt);
void eskf156_fb(kf_state_t *s, ins_t *ins, uint32_t opt);

void eskf156_mes_posR_set(kf_meas_t *m, nl_t *RposENU);
void eskf156_mes_velR_set(kf_meas_t *m, nl_t *RvelENU);
void eskf156_mes_zaruR_set(kf_meas_t *m, nl_t R);
void eskf156_mes_pos_set(kf_meas_t *m, ins_t *ins, nl_t *pos_enu, nl_t gnss_delay);
void eskf156_mes_vel_set(kf_meas_t *m, ins_t *ins, nl_t *vel_enu, nl_t gnss_delay);
void eskf156_dualgnssH_set(kf_meas_t *m,  nl_t *vec_n);
void eskf156_dualgnssZ_set(kf_meas_t *m, ins_t *ins,  nl_t *bl_vec,  nl_t *antA,  nl_t *antB);
void eskf_gravity_mes_set(kf_meas_t *m, ins_t *ins, nl_t *acc);
void eskf_mag_mes_set(kf_meas_t *m, ins_t *ins, nl_t *mag);

void eskf_zupt_mes_set(kf_meas_t *m, ins_t *ins);
void eskf_zaru_mes_set(kf_meas_t *m, ins_t *ins, nl_t *wb);
void eskf156_nhc_set(kf_meas_t *m, ins_t *ins);
void eskf156_od_set(kf_meas_t *m, ins_t *ins, nl_t od_speed);


void eskfatt_create(kf_state_t *s, kf_meas_t *gravity, kf_meas_t *mag, kf_meas_t *zaru, const eskfatt_t *opt);
void eskfatt_state_model(kf_state_t *s, ins_t *ins, nl_t dt);
void eskfatt_fb(kf_state_t *s, ins_t *ins, uint32_t opt);

//void gnss_spp(obs_t *obs, nav_t *nav, gnss_sat_t *sat, gnss_sol_t *sol);
//void gnss_rtk(obs_t *obs_rover, obs_t *obs_base, nav_t *nav);


void lla2enu(ins_t *ins, double *lla, nl_t *enu);
nl_t lla2enu2(double lat0, double lon0, double hgt0, double lat1, double lon1, double hgt1);
void enu2lla(ins_t *ins, nl_t *enu, double *lla);
void ecef2lla(const double *r, double *lla);
void lla2ecef(const double *lla, double *r);
void nl_hpl2vec(nl_t heading, nl_t pitch, nl_t len, nl_t *enu);
void nl_vec2hpl(nl_t *enu, nl_t *heading, nl_t *pitch, nl_t *len);
nl_t yaw_convert_npi_pi_to_2pi(nl_t yaw_npi_pi);
nl_t yaw_convert_2pi_to_npi_pi(nl_t yaw_0_2pi);
void nl_deg2dms(double deg, double *dms, int ndec);
void nl_time2epoch(nl_gtime_t t, nl_t *ep);
nl_gtime_t nl_epoch2time(const double *ep);
nl_gtime_t nl_gpst2time(int week, double sec);
double nl_time2gpst(nl_gtime_t t, int *week);
nl_gtime_t nl_utc2gpst(nl_gtime_t t);
double nl_timediff(nl_gtime_t t1, nl_gtime_t t2);
nl_gtime_t nl_gpst2utc(nl_gtime_t t);
nl_gtime_t nl_timeadd(nl_gtime_t t, double sec);


int nl_satno(int sys, int prn);
int nl_satsys(int sat, int *prn);
int nl_satid2no(const char *id);
void nl_satno2id(int sat, char *id);

uint32_t nl_gnss_sol2str(char *buf, gnss_sol_t *sol);



void nl_imu_init(nl_imu_t *imu, nl_imu_ops_t *ops, uint32_t read_frq);
void nl_calib_check(nl_imu_t *imu);
int nl_imu_read(nl_imu_t *imu);

void nl_imu_crst_all(nl_imu_t *imu);

void nl_vc_add(nl_var_calc_t *vc, nl_t *buf, uint32_t len);
void nl_vc_clear(nl_var_calc_t *vc);
void nl_vc_get_result(nl_var_calc_t *vc);

double lla2err(double *lla, double *lla0);

int ellipsoid_fit4(m_t *D, m_t *C, nl_t *B, nl_t *norm, nl_t *fit_err);
int ellipsoid_fit7(m_t *D, m_t *C, nl_t *B, nl_t *norm, nl_t *fit_err);

void mahony_gravity_fb(nl_t *q, nl_t *acc, nl_t *e);
void mahony_mag_fb(nl_t *q, nl_t *mag, nl_t *e);

/* debug and helper check */
void nl_assert_handler(const char *ex_string, const char *func, size_t line);
uint32_t nl_check_vec(nl_t *v1, nl_t *v2, uint32_t len, nl_t tol);


/* nl_app */
void nl_adj_att(nl_t *qin, att_t *att_tgt, nl_t *qout);

#ifdef __cplusplus
}
#endif

#endif
