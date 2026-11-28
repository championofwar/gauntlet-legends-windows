/* Shared CPU semantics. The static build specializes word at compile time.
   Only generated functions pass constant instruction words to this definition. */
static __attribute__((always_inline)) inline void execute_word(U at, U w){
    U opcode=w>>26;
    if(!(cp0[12]&0x20000000) && (opcode==17 || opcode==49 || opcode==53 || opcode==57 || opcode==61)){
        exception(11,0x10000000);return;
    }
    delay_slot=0;pc=next;next+=4;
    U op=w>>26,s=w>>21&31,t=w>>16&31,d=w>>11&31,k=w>>6&31,f=w&63;
    U a=reg[s],b=reg[t],u=w&65535,imm=(U)(S)(int16_t)u,addr=a+imm;
    int take=0;
    switch(op){
    case 0:switch(f){
        case 0:reg[d]=sx(b<<k);break;case 2:reg[d]=sx(b>>k);break;case 3:reg[d]=sx((S)b>>k);break;
        case 4:reg[d]=sx(b<<(a&31));break;case 6:reg[d]=sx(b>>(a&31));break;case 7:reg[d]=sx((S)b>>(a&31));break;
        case 8:next=a;break;case 9:reg[d]=sx(at+8);next=a;break;
        case 16:reg[d]=hi;break;case 17:hi=reg[s];break;case 18:reg[d]=lo;break;case 19:lo=reg[s];break;
        case 24:case 25:{uint64_t m=f==24?(uint64_t)((int64_t)(S)a*(S)b):(uint64_t)a*b;lo=sx((U)m);hi=sx((U)(m>>32));break;}
        case 26:{int64_t x=(S)a,y=(S)b;lo=sx(y?(U)(x/y):(x<0?1u:~0u));hi=sx(y?(U)(x%y):a);break;}
        case 27:lo=sx(b?a/b:~0u);hi=sx(b?a%b:a);break;
        case 32:if((~(a^b)&(a^(a+b)))>>31){status=OPCODE;break;}reg[d]=sx(a+b);break;
        case 33:reg[d]=sx(a+b);break;case 35:reg[d]=sx(a-b);break;
        case 36:reg[d]=reg[s]&reg[t];break;case 37:reg[d]=reg[s]|reg[t];break;case 38:reg[d]=reg[s]^reg[t];break;case 39:reg[d]=~(reg[s]|reg[t]);break;
        case 42:reg[d]=(int64_t)reg[s]<(int64_t)reg[t];break;case 43:reg[d]=reg[s]<reg[t];break;
        default:status=OPCODE;
    }break;
    case 1:
        if(t&~19u){status=OPCODE;break;}
        take=(t&1)?(int64_t)reg[s]>=0:(int64_t)reg[s]<0;
        // REGIMM link variants write RA even when the branch falls through.
        if(t&16)reg[31]=sx(at+8);
        if((t&2) && !take){pc=next;next+=4;}
        break;
    case 2:case 3:if(op==3)reg[31]=sx(at+8);next=((at+4)&0xf0000000)|((w&0x3ffffff)<<2);break;
    case 4:case 20:take=reg[s]==reg[t];break;case 5:case 21:take=reg[s]!=reg[t];break;
    case 6:case 22:take=(S)a<=0;break;case 7:case 23:take=(S)a>0;break;
    case 8:if((~(a^imm)&(a^addr))>>31){status=OPCODE;break;} /* fall through */
    case 9:reg[t]=sx(addr);break;case 10:reg[t]=(int64_t)reg[s]<(S)imm;break;case 11:reg[t]=reg[s]<sx(imm);break;
    case 12:reg[t]=a&u;break;case 13:reg[t]=reg[s]|u;break;case 14:reg[t]=reg[s]^u;break;case 15:reg[t]=sx(u<<16);break;
    case 16:
        if(s==0)reg[t]=cp0[d];else if(s==4){cp0[d]=b;if(d==11)cp0[13]&=~0x8000u;}
        else if(s==16 && f==2){unsigned i=cp0[0]&31;tlb[i].hi=cp0[10];tlb[i].mask=cp0[5];tlb[i].lo[0]=cp0[2];tlb[i].lo[1]=cp0[3];for(int j=0;j<128;j++)pages[j].tag=0;}
        else if(s==16 && f==8){
            cp0[0]=0x80000000;
            for(U i=0;i<32;i++)if(!((cp0[10]^tlb[i].hi)&~(tlb[i].mask|0x1fff))){cp0[0]=i;break;}
        }else if(s==16 && f==1){
            U i=cp0[0]&31;cp0[5]=tlb[i].mask;cp0[10]=tlb[i].hi;cp0[2]=tlb[i].lo[0];cp0[3]=tlb[i].lo[1];
        }else if(s==16 && f==24){pc=cp0[14];next=pc+4;cp0[12]&=~2u;dispatches++;}
        else status=OPCODE;
        break;
    case 17:if(s==0)reg[t]=sx(fp[d]);else if(s==4)fp[d]=b;else if(s==8){take=((fcsr>>23)& 1)==(t& 1);if(!take && (t& 2)){pc=next;next+=4;}}else if(s>=16)floating(s,t,d,k,f);else if(s==2 && d==31)reg[t]=fcsr;else if(s==6 && d==31)fcsr=b;else status=OPCODE;break;
    case 32:reg[t]=sx((S)(int8_t)access_bus(addr,1,0,0));break;case 33:reg[t]=sx((S)(int16_t)access_bus(addr,2,0,0));break;
    case 35:reg[t]=sx(access_bus(addr,4,0,0));break;case 36:reg[t]=access_bus(addr,1,0,0);break;case 37:reg[t]=access_bus(addr,2,0,0);break;
    case 34:case 38:case 42:case 46:{
        U m=access_bus(addr&~3u,4,0,0),shift=(addr&3)*8;
        if(op==34)reg[t]=sx((m<<shift)|(b&~(~0u<<shift)));
        if(op==38)reg[t]=sx((m>>(24-shift))|(b&~(~0u>>(24-shift))));
        if(op==42)access_bus(addr&~3u,4,(b>>shift)|(m&~(~0u>>shift)),1);
        if(op==46)access_bus(addr&~3u,4,(b<<(24-shift))|(m&~(~0u<<(24-shift))),1);
        break;
    }
    case 40:access_bus(addr,1,b,1);break;case 41:access_bus(addr,2,b,1);break;case 43:access_bus(addr,4,b,1);break;
    case 48:reg[t]=sx(access_bus(addr,4,0,0));reservation=physical(addr);break;
    case 56:if(reservation==physical(addr)){access_bus(addr,4,b,1);reg[t]=1;}else reg[t]=0;reservation=~0u;break;
    case 49:fp[t]=access_bus(addr,4,0,0);break;
    case 57:access_bus(addr,4,fp[t],1);break;
    case 53:case 61:
        if((addr&7)||(t&1)){status=ALIGNMENT;stop_address=addr;break;}
        if(op==53){fp[t+1]=access_bus(addr,4,0,0);fp[t]=access_bus(addr+4,4,0,0);}
        else{access_bus(addr,4,fp[t+1],1);access_bus(addr+4,4,fp[t],1);}break;
    case 55:case 63:
        if(addr&7){status=ALIGNMENT;stop_address=addr;break;}
        if(op==55){Q upper=access_bus(addr,4,0,0);reg[t]=(upper<<32)|access_bus(addr+4,4,0,0);}
        else {access_bus(addr,4,reg[t]>>32,1);access_bus(addr+4,4,b,1);}
        break;
    case 47:break; // Instruction and data share the same coherent RAM.
    default:status=OPCODE;
    }
    delay_slot=(op>=1 && op<=7)||(op>=20 && op<=23)||(op==0 && (f==8||f==9))||(op==17 && s==8);
    if(!take && ((op>=20&&op<=23)||(op==1 && (t&2))||(op==17 && s==8 && (t&2))))delay_slot=0;
    if(take)next=at+4+(imm<<2);
    else if(op>=20 && op<=23){pc=next;next+=4;}
    reg[0]=0;
}
