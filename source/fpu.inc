typedef union {U bits;float v;} Single;
typedef union {Q bits;double v;} Double;
static double get_double(unsigned d){Double n={.bits=(Q)fp[d+1]<<32|fp[d]};return n.v;}
static void set_double(unsigned d,double v){Double n={.v=v};fp[d]=n.bits;fp[d+1]=n.bits>>32;}
void floating(U fmt,U t,U d,U k,U f){
    double a,b,v=0;
    if(fmt==17 && ((d|t)&1)){status=OPCODE;return;}
    if(fmt==16){a=((Single){.bits=fp[d]}).v;b=((Single){.bits=fp[t]}).v;}
    else if(fmt==17){a=get_double(d);b=get_double(t);}
    else if(fmt==20){a=(S)fp[d];b=0;}
    else{status=OPCODE;return;}
    switch(f){
    case 0:v=a+b;break;case 1:v=a-b;break;case 2:v=a*b;break;case 3:v=a/b;break;
    case 4:v=__builtin_sqrt(a);break;case 5:v=__builtin_fabs(a);break;case 6:v=a;break;case 7:v=-a;break;
    case 13:fp[k]=(S)a;return;
    case 32:fp[k]=((Single){.v=(float)a}).bits;return;
    case 33:if(k&1){status=OPCODE;return;}set_double(k,a);return;
    case 36:fp[k]=(S)__builtin_nearbyint(a);return;
    default:
        if(f>=48){int result=((f&4)&&a<b)||((f&2)&&a==b)||((f&1)&&(a!=a||b!=b));fcsr=(fcsr&~0x800000u)|(result?0x800000:0);return;}
        status=OPCODE;return;
    }
    if(fmt==16)fp[k]=((Single){.v=(float)v}).bits;else if(!(k&1))set_double(k,v);else status=OPCODE;
}
