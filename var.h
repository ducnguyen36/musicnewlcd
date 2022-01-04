
//cmd enum Ma Mode Lenh
enum CMDCODE {
    NONE,
    READ,
    ADD,
    DEL,
    RESET,
    FACTORY,
    DEF,
    SONG,
    STOP,
    PLAY,
    MAN,
    AUTO,
    STC,
    GPS,
    RTC,
    COM,
    TK,
    PHONE,
    PRINT,
    INFO,
    SIM,
    ATC,
    KDL
};
// #define READ        1
// #define ADD         2
// #define DEL         3
// #define RESET       4
// #define DEF         5
// #define SONG        6
// #define STOP        7
// #define PLAY        8
// #define MAN         9
// #define AUTO        10
// #define STC         11
// #define GPS         12
// #define RTC         13stop

// #define COM         14
// #define SMS         15
// #define TK          16
// #define PHONE       17
// #define PRINT       18
// #define INFO        19
// #define ATC         20
// #define FACTORY     21

//bien cho in Serial
#define P_SERIAL    2
#define P_SERIAL1   4
#define P_GSM       1

//Error Code Ma Loi
#define TEMP_ERROR      1   //Loi khong mo duoc file temp
#define OPEN_ERROR      2   //Loi khong mo duoc file chinh
#define REMO_ERROR      3   //Loi khong xoa duoc file
#define RENA_ERROR      4   //Loi khong doi ten duoc file
#define NAME_ERROR      5   //Loi ten file khong dung
#define SRC_ERROR       6   //Loi khong mo duoc file nguon
#define DES_ERROR       7   //Loi khong mo duoc file dich
#define PLAY_ERROR      8   //Loi play nhac MP3
#define ROOT_ERROR      9   //Loi mo thu muc root
#define PLAYING_ERROR   10  //Loi nhac dang choi khong the truy xuat file
#define MP3_ERROR       11  //Loi khoi tao module Mp3
#define LICH_ERROR      12  //Loi doc file lichnhac.txt
#define TIME_ERROR      13  //Loi format thoi gian
#define CONN_ERROR      14  //Loi ket noi module sim800
#define SIM_ERROR       15  //Loi khong co the sim
#define SMS_ERROR       16  //Loi gui tin nhan sms
#define MASTER_ERROR    17  //Loi quyen master

//MA LOI THOI GIAN
#define LOI_GIO      0x01
#define LOI_PHUT     0x02
#define LOI_GIAY     0x04
#define LOI_NGAY     0x08
#define LOI_THANG    0x10
#define LOI_NAM      0x20

//Cai Dat Chan
#define IR_RECEIVE_PIN      22   //Chan nhan hong ngoai
#define RELAY               24   //Chan Relay
#define BUTTON              23   //Chan phim

//Trang Thai Phim
#define IDLE                0
#define PRESSED             1
#define RELEASED            2
#define HOLD                3