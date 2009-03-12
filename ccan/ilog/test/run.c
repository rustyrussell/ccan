#include <stdio.h>
#include "ilog/ilog.h"
#include "tap/tap.h"
#if defined(__GNUC_PREREQ)
# if __GNUC_PREREQ(4,2)
#  pragma GCC diagnostic ignored "-Wparentheses"
# endif
#endif

/*Dead simple (but slow) versions to compare against.*/

static int test_ilog32(uint32_t _v){
  int ret;
  for(ret=0;_v;ret++)_v>>=1;
  return ret;
}

static int test_ilog64(uint64_t _v){
  int ret;
  for(ret=0;_v;ret++)_v>>=1;
  return ret;
}

#define NTRIALS (64)

int main(int _argc,const char *_argv[]){
  int nmatches;
  int i;
  int j;
  /*This is how many tests you plan to run.*/
  plan_tests(2);
  nmatches=0;
  for(i=0;i<=32;i++){
    uint32_t v;
    /*Test each bit in turn (and 0).*/
    v=i?(uint32_t)1U<<i-1:0;
    for(j=0;j<NTRIALS;j++){
      int l;
      l=test_ilog32(v);
      if(ILOG_32(v)!=l){
        fprintf(stderr,"ILOG_32(0x%08lX): %i != %i\n",(long)v,ILOG_32(v),l);
      }
      else nmatches++;
      if(ilog32(v)!=l){
        fprintf(stderr,"ilog32(0x%08lX): %i != %i\n",(long)v,ilog32(v),l);
      }
      else nmatches++;
      if(STATIC_ILOG_32(v)!=l){
        fprintf(stderr,"STATIC_ILOG_32(0x%08lX): %i != %i\n",
         (long)v,STATIC_ILOG_32(v),l);
      }
      else nmatches++;
      /*Also try a few more pseudo-random values with at most the same number
         of bits.*/
      v=1103515245U*v+12345U&0xFFFFFFFFU>>(33-i>>1)>>(32-i>>1);
    }
  }
  ok1(nmatches==3*(32+1)*NTRIALS);
  nmatches=0;
  for(i=0;i<=64;i++){
    uint64_t v;
    /*Test each bit in turn (and 0).*/
    v=i?(uint64_t)1U<<i-1:0;
    for(j=0;j<NTRIALS;j++){
      int l;
      l=test_ilog64(v);
      if(ILOG_64(v)!=l){
        fprintf(stderr,"ILOG_64(0x%016llX): %i != %i\n",
         (long long)v,ILOG_64(v),l);
      }
      else nmatches++;
      if(ilog64(v)!=l){
        fprintf(stderr,"ilog64(0x%016llX): %i != %i\n",
         (long long)v,ilog64(v),l);
      }
      else nmatches++;
      if(STATIC_ILOG_64(v)!=l){
        fprintf(stderr,"STATIC_ILOG_64(0x%016llX): %i != %i\n",
         (long long)v,STATIC_ILOG_64(v),l);
      }
      else nmatches++;
      /*Also try a few more pseudo-random values with at most the same number
         of bits.*/
      v=(uint64_t)(2862933555777941757ULL*v+3037000493ULL
       &0xFFFFFFFFFFFFFFFFULL>>(65-i>>1)>>(64-i>>1));
    }
  }
  ok1(nmatches==3*(64+1)*NTRIALS);
  return exit_status();
}
