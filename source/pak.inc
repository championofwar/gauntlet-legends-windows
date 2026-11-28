/* Player one's single-bank Controller Pak. */
B pak_data[32768];
int pak_ready,pak_dirty;
void pak_format(void){
    for(unsigned i=0;i<sizeof(pak_data);i++)pak_data[i]=0;
    B id[32]={0};
    id[7]=0x65;id[15]=0xa1;id[23]=0x74;id[25]=1;id[26]=1;
    unsigned sum=0;
    for(int i=0;i<28;i+=2)sum+=256*id[i]+id[i+1];
    id[28]=sum>>8;id[29]=sum;sum=0xfff2-sum;id[30]=sum>>8;id[31]=sum;
    const int blocks[]={1,3,4,6};
    for(int i=0;i<4;i++)for(int j=0;j<32;j++)pak_data[blocks[i]*32+j]=id[j];
    for(int bank=1;bank<=2;bank++){
        B *index=pak_data+bank*256;index[1]=(123*3)&255;
        for(int page=5;page<128;page++)index[2*page+1]=3;
    }
}
static B pak_crc(const B *block){
    unsigned crc=0;
    for(int i=0;i<=32;i++)for(int bit=7;bit>=0;bit--){
        unsigned top=crc&128;crc=(crc<<1)|(i<32?((block[i]>>bit)&1):0);
        if(top)crc^=0x85;
    }
    return (B)crc;
}
int pak_motor;
static int motor_accessory,motor_bank;
void pak_select_motor(int enabled){
    if(enabled!=motor_accessory){motor_accessory=enabled;motor_bank=0;pak_motor=0;}
}
static void pak_block(const B *cmd,B *reply){
    unsigned pos=(cmd[1]<<8)|(cmd[2]&224);
    if(motor_accessory){
        if(cmd[0]==2){
            unsigned value=pos>=0x8000 && pos<0x9000 && motor_bank==0x80?0x80:0;
            for(int j=0;j<32;j++)reply[j]=value;
            reply[32]=pak_crc(reply);
        }else{
            if(pos==0x8000)motor_bank=cmd[3];
            if(pos==0xc000)pak_motor=cmd[3]!=0;
            reply[0]=pak_crc(cmd+3);
        }
        return;
    }
    if(cmd[0]==2){
        for(int j=0;j<32;j++)reply[j]=pos<32768?pak_data[pos+j]:0;
        reply[32]=pak_crc(reply);
    }else{
        if(pos<32768)for(int j=0;j<32;j++)if(pak_data[pos+j]!=cmd[3+j]){
            pak_data[pos+j]=cmd[3+j];pak_dirty=1;
        }
        reply[0]=pak_crc(cmd+3);
    }
}
