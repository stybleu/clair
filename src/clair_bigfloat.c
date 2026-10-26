#include "clair_bigfloat.h"
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checked_add_i64(int64_t a, int64_t b, int64_t *out) {
    if (__builtin_add_overflow(a, b, out)) return CLAIR_BIG_TOO_LARGE;
    return CLAIR_BIG_OK;
}
static int checked_sub_i64(int64_t a, int64_t b, int64_t *out) {
    if (__builtin_sub_overflow(a, b, out)) return CLAIR_BIG_TOO_LARGE;
    return CLAIR_BIG_OK;
}
static void swap_bf(ClairBigFloat *a, ClairBigFloat *b) { ClairBigFloat tmp = *a; *a=*b; *b=tmp; }
void clair_bf_init(ClairBigFloat *x) { clair_big_init(&x->coefficient); x->exp10=0; }
void clair_bf_clear(ClairBigFloat *x) { clair_big_clear(&x->coefficient); x->exp10=0; }

/* Build normalized coefficient and decimal exponent. Input coefficient already signed.
   'sticky' is used by division to indicate omitted nonzero digits below the guard digits. */
static int from_coeff(ClairBigFloat *out, const ClairBigInt *coefficient,
                      int64_t exponent, uint32_t precision, int sticky) {
    if (!precision || precision > 1000000) return CLAIR_BIG_INVALID;
    ClairBigFloat result; clair_bf_init(&result);
    if (!coefficient->sign) {swap_bf(out, &result); clair_bf_clear(&result);return CLAIR_BIG_OK;}
    char *raw=NULL;
    int rc=clair_big_to_decimal(coefficient, &raw);
    if (rc) return rc;
    const int negative = raw[0]=='-';
    char *digits=raw+negative;
    size_t count=strlen(digits), keep=count;
    if (count>precision) {
        keep=precision;
        int greater=digits[keep]>'5';
        int equal=digits[keep]=='5';
        int later=sticky;
        for (size_t i=keep+1;i<count;i++) if (digits[i]!='0') {later=1;break;}
        int increment=greater || (equal && (later || ((digits[keep-1]-'0')&1)));
        if (count-keep>(size_t)INT64_MAX) {rc=CLAIR_BIG_TOO_LARGE;goto done;}
        rc=checked_add_i64(exponent,(int64_t)(count-keep),&exponent);
        if(rc)goto done;
        digits[keep]='\0';
        if(increment) {
            size_t k=keep;
            while(k>0 && digits[k-1]=='9') {digits[--k]='0';}
            if(k>0) digits[k-1]++;
            else {
                memmove(digits+1,digits,keep+1);
                digits[0]='1';
                keep++;
            }
        }
    }
    /* Strip unneeded decimal zeros: coefficient remains minimal. */
    while(keep>1 && digits[keep-1]=='0') {
        keep--; digits[keep]='\0';
        rc=checked_add_i64(exponent,1,&exponent);
        if(rc)goto done;
    }
    if (negative) {
        /* Reconstruct a signed decimal without writing before the digits. */
        size_t n=keep+2;
        char *signed_digits=(char*)malloc(n);
        if(!signed_digits){rc=CLAIR_BIG_NOMEM;goto done;}
        signed_digits[0]='-';memcpy(signed_digits+1,digits,keep+1);
        rc=clair_big_from_decimal(&result.coefficient,signed_digits);
        free(signed_digits);
    } else rc=clair_big_from_decimal(&result.coefficient,digits);
    if(rc)goto done;
    result.exp10=exponent;
    swap_bf(out,&result);
done:
    free(raw);clair_bf_clear(&result);return rc;
}

int clair_bf_from_scaled(ClairBigFloat *out, const ClairBigInt *coefficient, int64_t exp10,
                         uint32_t precision, int sticky) {
    return from_coeff(out, coefficient, exp10, precision, sticky);
}

int clair_bf_parse(ClairBigFloat *out, const char *s, uint32_t precision) {
    if(!s || !*s || !precision || precision>1000000) return CLAIR_BIG_INVALID;
    const char *p=s;
    int negative=0;
    if(*p=='-' || *p=='+'){negative=*p=='-';p++;}
    size_t len=strlen(p);
    if(len>SIZE_MAX-3)return CLAIR_BIG_TOO_LARGE;
    char *digits=(char*)malloc(len+3);
    if(!digits)return CLAIR_BIG_NOMEM;
    size_t n=0,fraction=0;
    int saw_digit=0, dot=0;
    while(*p && *p!='e' && *p!='E') {
        if(*p=='.') {if(dot){free(digits);return CLAIR_BIG_INVALID;}dot=1;}
        else if(*p>='0' && *p<='9') {digits[n++]=*p;if(dot)fraction++;saw_digit=1;}
        else {free(digits);return CLAIR_BIG_INVALID;}
        p++;
    }
    if(!saw_digit || fraction>(size_t)INT64_MAX){free(digits);return CLAIR_BIG_INVALID;}
    int64_t exponent=0;
    if(*p) {
        p++;
        int negexp=0;
        if(*p=='-' || *p=='+'){negexp=*p=='-';p++;}
        if(!*p) {free(digits);return CLAIR_BIG_INVALID;}
        uint64_t x=0, max=negexp ? (uint64_t)INT64_MAX+1u:(uint64_t)INT64_MAX;
        for(;*p;p++) {
            if(*p<'0'||*p>'9') {free(digits);return CLAIR_BIG_INVALID;}
            unsigned d=(unsigned)(*p-'0');
            if(x>(max-d)/10u) {free(digits);return CLAIR_BIG_TOO_LARGE;}
            x=x*10u+d;
        }
        exponent=negexp ? (x==(uint64_t)INT64_MAX+1u ? INT64_MIN:-(int64_t)x):(int64_t)x;
    }
    int rc=checked_sub_i64(exponent,(int64_t)fraction,&exponent);
    if(rc){free(digits);return rc;}
    digits[n]='\0';
    ClairBigInt coeff;clair_big_init(&coeff);
    char *text=digits;
    if(negative){memmove(digits+1,digits,n+1);digits[0]='-';text=digits;}
    rc=clair_big_from_decimal(&coeff,text);
    if(!rc)rc=from_coeff(out,&coeff,exponent,precision,0);
    clair_big_clear(&coeff);free(digits);return rc;
}

int clair_bf_format(const ClairBigFloat *x, char **out) {
    if (!out)return CLAIR_BIG_INVALID;
    *out=NULL;
    char *raw=NULL;
    int rc=clair_big_to_decimal(&x->coefficient,&raw);if(rc)return rc;
    if(!x->coefficient.sign){*out=raw;return 0;}
    size_t neg=raw[0]=='-',nd=strlen(raw+neg);
    int64_t e;
    rc=checked_add_i64(x->exp10,(int64_t)(nd-1),&e);
    if(rc){free(raw);return rc;}
    char suffix[48];snprintf(suffix,sizeof(suffix),"e%" PRId64,e);
    size_t sl=strlen(suffix);
    if (nd>SIZE_MAX-sl-5) {free(raw);return CLAIR_BIG_TOO_LARGE;}
    char *buf=(char*)malloc(nd+sl+5+neg);
    if(!buf){free(raw);return CLAIR_BIG_NOMEM;}
    char *p=buf;
    if(neg)*p++='-';
    *p++=raw[neg];
    if(nd>1){*p++='.';memcpy(p,raw+neg+1,nd-1);p+=nd-1;}
    memcpy(p,suffix,sl+1);
    *out=buf;free(raw);return 0;
}
static int clair_bf_pow10(ClairBigInt *out, uint64_t exponent) {
    ClairBigInt base;clair_big_init(&base);
    int rc=clair_big_from_i64(&base,10);
    if(!rc)rc=clair_big_pow_u64(out,&base,exponent);
    clair_big_clear(&base);return rc;
}
static int scale_coeff(ClairBigInt *out,const ClairBigInt *a,uint64_t shift) {
    if(!a->sign)return clair_big_from_i64(out,0);
    if(!shift)return clair_big_copy(out,a);
    ClairBigInt power;clair_big_init(&power);
    int rc=clair_bf_pow10(&power,shift);
    if(!rc)rc=clair_big_mul(out,a,&power);
    clair_big_clear(&power);return rc;
}
static int decimal_order(const ClairBigFloat *x, int64_t *result) {
    if(!x->coefficient.sign){*result=INT64_MIN;return 0;}
    char *digits=NULL;int rc=clair_big_to_decimal(&x->coefficient,&digits);
    if(rc)return rc;
    size_t n=strlen(digits+(digits[0]=='-'));
    free(digits);
    if(n-1>(size_t)INT64_MAX)return CLAIR_BIG_TOO_LARGE;
    return checked_add_i64(x->exp10,(int64_t)(n-1),result);
}
static int add_impl(ClairBigFloat *out,const ClairBigFloat *a,const ClairBigFloat *b,
                    uint32_t prec,int subtract) {
    if(!prec || prec>1000000)return CLAIR_BIG_INVALID;
    /* When orders differ by more than precision+2, the smaller value cannot
       change the correctly rounded result of the larger value in this prototype. */
    int64_t oa,ob;int rc=decimal_order(a,&oa);if(rc)return rc;
    rc=decimal_order(b,&ob);if(rc)return rc;
    if(!a->coefficient.sign) {
        ClairBigInt neg=b->coefficient;neg.sign=subtract?-neg.sign:neg.sign;
        return from_coeff(out,&neg,b->exp10,prec,0);
    }
    if(!b->coefficient.sign)return from_coeff(out,&a->coefficient,a->exp10,prec,0);
    if(oa>ob && (uint64_t)oa-(uint64_t)ob>(uint64_t)prec+3u)
        return from_coeff(out,&a->coefficient,a->exp10,prec,0);
    if(ob>oa && (uint64_t)ob-(uint64_t)oa>(uint64_t)prec+3u) {
        ClairBigInt neg=b->coefficient;neg.sign=subtract?-neg.sign:neg.sign;
        return from_coeff(out,&neg,b->exp10,prec,0);
    }
    int64_t e=a->exp10<b->exp10?a->exp10:b->exp10;
    /* These gaps are at most approximately 2*precision, after the order check. */
    uint64_t ka=(uint64_t)a->exp10-(uint64_t)e,kb=(uint64_t)b->exp10-(uint64_t)e;
    ClairBigInt ac,bc,sum;clair_big_init(&ac);clair_big_init(&bc);clair_big_init(&sum);
    rc=scale_coeff(&ac,&a->coefficient,ka);if(rc)goto end;
    rc=scale_coeff(&bc,&b->coefficient,kb);if(rc)goto end;
    rc=subtract?clair_big_sub(&sum,&ac,&bc):clair_big_add(&sum,&ac,&bc);
    if(!rc)rc=from_coeff(out,&sum,e,prec,0);
end:
    clair_big_clear(&ac);clair_big_clear(&bc);clair_big_clear(&sum);return rc;
}
int clair_bf_add(ClairBigFloat *o,const ClairBigFloat *a,const ClairBigFloat *b,uint32_t p){return add_impl(o,a,b,p,0);}
int clair_bf_sub(ClairBigFloat *o,const ClairBigFloat *a,const ClairBigFloat *b,uint32_t p){return add_impl(o,a,b,p,1);}
int clair_bf_mul(ClairBigFloat *out,const ClairBigFloat *a,const ClairBigFloat *b,uint32_t precision){
    if(!precision || precision>1000000)return CLAIR_BIG_INVALID;
    int64_t e;int rc=checked_add_i64(a->exp10,b->exp10,&e);if(rc)return rc;
    ClairBigInt prod;clair_big_init(&prod);
    rc=clair_big_mul(&prod,&a->coefficient,&b->coefficient);
    if(!rc)rc=from_coeff(out,&prod,e,precision,0);
    clair_big_clear(&prod);return rc;
}
int clair_bf_div(ClairBigFloat *out,const ClairBigFloat *a,const ClairBigFloat *b,uint32_t precision){
    if(!precision || precision>1000000)return CLAIR_BIG_INVALID;
    if(!b->coefficient.sign)return CLAIR_BIG_DIVZERO;
    if(!a->coefficient.sign){ClairBigFloat z;clair_bf_init(&z);swap_bf(out,&z);clair_bf_clear(&z);return 0;}
    char *as=NULL,*bs=NULL;
    int rc=clair_big_to_decimal(&a->coefficient,&as);if(rc)return rc;
    rc=clair_big_to_decimal(&b->coefficient,&bs);if(rc){free(as);return rc;}
    int64_t da=(int64_t)strlen(as+(as[0]=='-'));
    int64_t db=(int64_t)strlen(bs+(bs[0]=='-'));
    free(as);free(bs);
    int64_t shift=(int64_t)precision+3 + db-da;
    int64_t e;
    rc=checked_sub_i64(a->exp10,b->exp10,&e);
    if(rc)return rc;
    rc=checked_sub_i64(e,shift,&e);
    if(rc)return rc;
    ClairBigInt num,den,q,r;
    clair_big_init(&num);clair_big_init(&den);clair_big_init(&q);clair_big_init(&r);
    if(shift>=0){rc=scale_coeff(&num,&a->coefficient,(uint64_t)shift);if(!rc)rc=clair_big_copy(&den,&b->coefficient);}
    else {rc=clair_big_copy(&num,&a->coefficient);if(!rc)rc=scale_coeff(&den,&b->coefficient,(uint64_t)(-shift));}
    if(!rc)rc=clair_big_divmod(&q,&r,&num,&den);
    if(!rc)rc=from_coeff(out,&q,e,precision,r.sign!=0);
    clair_big_clear(&num);clair_big_clear(&den);clair_big_clear(&q);clair_big_clear(&r);return rc;
}
