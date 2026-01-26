/*
 * ira - iRacing Application
 * iRacing SDK Definitions (C-compatible)
 *
 * Adapted from the official iRacing SDK
 * Original Copyright (c) 2013, iRacing.com Motorsport Simulations, LLC.
 */

#ifndef IRA_IRSDK_DEFINES_H
#define IRA_IRSDK_DEFINES_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Memory mapped file names */
#define IRSDK_DATAVALIDEVENTNAME    "Local\\IRSDKDataValidEvent"
#define IRSDK_MEMMAPFILENAME        "Local\\IRSDKMemMapFileName"
#define IRSDK_BROADCASTMSGNAME      "IRSDK_BROADCASTMSG"

/* Constants */
#define IRSDK_MAX_BUFS      4
#define IRSDK_MAX_STRING    32
#define IRSDK_MAX_DESC      64

#define IRSDK_UNLIMITED_LAPS    32767
#define IRSDK_UNLIMITED_TIME    604800.0f

/* SDK Version */
#define IRSDK_VER   2

/* Status field bitflags */
typedef enum {
    IRSDK_ST_CONNECTED = 1
} irsdk_StatusField;

/* Variable types */
typedef enum {
    IRSDK_TYPE_CHAR = 0,
    IRSDK_TYPE_BOOL,
    IRSDK_TYPE_INT,
    IRSDK_TYPE_BITFIELD,
    IRSDK_TYPE_FLOAT,
    IRSDK_TYPE_DOUBLE,
    IRSDK_TYPE_COUNT
} irsdk_VarType;

/* Size in bytes for each variable type */
static const int irsdk_var_type_bytes[IRSDK_TYPE_COUNT] = {
    1,  /* IRSDK_TYPE_CHAR */
    1,  /* IRSDK_TYPE_BOOL */
    4,  /* IRSDK_TYPE_INT */
    4,  /* IRSDK_TYPE_BITFIELD */
    4,  /* IRSDK_TYPE_FLOAT */
    8   /* IRSDK_TYPE_DOUBLE */
};

/* Track location */
typedef enum {
    IRSDK_TRKLOC_NOT_IN_WORLD = -1,
    IRSDK_TRKLOC_OFF_TRACK = 0,
    IRSDK_TRKLOC_IN_PIT_STALL,
    IRSDK_TRKLOC_APPROACHING_PITS,
    IRSDK_TRKLOC_ON_TRACK
} irsdk_TrkLoc;

/* Track surface */
typedef enum {
    IRSDK_TRKSURF_NOT_IN_WORLD = -1,
    IRSDK_TRKSURF_UNDEFINED = 0,
    IRSDK_TRKSURF_ASPHALT_1,
    IRSDK_TRKSURF_ASPHALT_2,
    IRSDK_TRKSURF_ASPHALT_3,
    IRSDK_TRKSURF_ASPHALT_4,
    IRSDK_TRKSURF_CONCRETE_1,
    IRSDK_TRKSURF_CONCRETE_2,
    IRSDK_TRKSURF_RACING_DIRT_1,
    IRSDK_TRKSURF_RACING_DIRT_2,
    IRSDK_TRKSURF_PAINT_1,
    IRSDK_TRKSURF_PAINT_2,
    IRSDK_TRKSURF_RUMBLE_1,
    IRSDK_TRKSURF_RUMBLE_2,
    IRSDK_TRKSURF_RUMBLE_3,
    IRSDK_TRKSURF_RUMBLE_4,
    IRSDK_TRKSURF_GRASS_1,
    IRSDK_TRKSURF_GRASS_2,
    IRSDK_TRKSURF_GRASS_3,
    IRSDK_TRKSURF_GRASS_4,
    IRSDK_TRKSURF_DIRT_1,
    IRSDK_TRKSURF_DIRT_2,
    IRSDK_TRKSURF_DIRT_3,
    IRSDK_TRKSURF_DIRT_4,
    IRSDK_TRKSURF_SAND,
    IRSDK_TRKSURF_GRAVEL_1,
    IRSDK_TRKSURF_GRAVEL_2,
    IRSDK_TRKSURF_GRASSCRETE,
    IRSDK_TRKSURF_ASTROTURF
} irsdk_TrkSurf;

/* Session state */
typedef enum {
    IRSDK_STATE_INVALID = 0,
    IRSDK_STATE_GET_IN_CAR,
    IRSDK_STATE_WARMUP,
    IRSDK_STATE_PARADE_LAPS,
    IRSDK_STATE_RACING,
    IRSDK_STATE_CHECKERED,
    IRSDK_STATE_COOLDOWN
} irsdk_SessionState;

/* Car left/right spotter info */
typedef enum {
    IRSDK_LR_OFF = 0,
    IRSDK_LR_CLEAR,
    IRSDK_LR_CAR_LEFT,
    IRSDK_LR_CAR_RIGHT,
    IRSDK_LR_CAR_LEFT_RIGHT,
    IRSDK_LR_2_CARS_LEFT,
    IRSDK_LR_2_CARS_RIGHT
} irsdk_CarLeftRight;

/* Pit service status */
typedef enum {
    IRSDK_PITSV_NONE = 0,
    IRSDK_PITSV_IN_PROGRESS,
    IRSDK_PITSV_COMPLETE,
    IRSDK_PITSV_TOO_FAR_LEFT = 100,
    IRSDK_PITSV_TOO_FAR_RIGHT,
    IRSDK_PITSV_TOO_FAR_FORWARD,
    IRSDK_PITSV_TOO_FAR_BACK,
    IRSDK_PITSV_BAD_ANGLE,
    IRSDK_PITSV_CANT_FIX_THAT
} irsdk_PitSvStatus;

/* Pace mode */
typedef enum {
    IRSDK_PACE_SINGLE_FILE_START = 0,
    IRSDK_PACE_DOUBLE_FILE_START,
    IRSDK_PACE_SINGLE_FILE_RESTART,
    IRSDK_PACE_DOUBLE_FILE_RESTART,
    IRSDK_PACE_NOT_PACING
} irsdk_PaceMode;

/* Track wetness */
typedef enum {
    IRSDK_WETNESS_UNKNOWN = 0,
    IRSDK_WETNESS_DRY,
    IRSDK_WETNESS_MOSTLY_DRY,
    IRSDK_WETNESS_VERY_LIGHTLY_WET,
    IRSDK_WETNESS_LIGHTLY_WET,
    IRSDK_WETNESS_MODERATELY_WET,
    IRSDK_WETNESS_VERY_WET,
    IRSDK_WETNESS_EXTREMELY_WET
} irsdk_TrackWetness;

/* Engine warnings (bitfield) */
typedef enum {
    IRSDK_ENG_WATER_TEMP_WARNING    = 0x0001,
    IRSDK_ENG_FUEL_PRESSURE_WARNING = 0x0002,
    IRSDK_ENG_OIL_PRESSURE_WARNING  = 0x0004,
    IRSDK_ENG_STALLED               = 0x0008,
    IRSDK_ENG_PIT_SPEED_LIMITER     = 0x0010,
    IRSDK_ENG_REV_LIMITER_ACTIVE    = 0x0020,
    IRSDK_ENG_OIL_TEMP_WARNING      = 0x0040,
    IRSDK_ENG_MAND_REP_NEEDED       = 0x0080,
    IRSDK_ENG_OPT_REP_NEEDED        = 0x0100
} irsdk_EngineWarnings;

/* Global flags (bitfield) */
typedef enum {
    /* Global flags */
    IRSDK_FLAG_CHECKERED        = 0x00000001,
    IRSDK_FLAG_WHITE            = 0x00000002,
    IRSDK_FLAG_GREEN            = 0x00000004,
    IRSDK_FLAG_YELLOW           = 0x00000008,
    IRSDK_FLAG_RED              = 0x00000010,
    IRSDK_FLAG_BLUE             = 0x00000020,
    IRSDK_FLAG_DEBRIS           = 0x00000040,
    IRSDK_FLAG_CROSSED          = 0x00000080,
    IRSDK_FLAG_YELLOW_WAVING    = 0x00000100,
    IRSDK_FLAG_ONE_LAP_TO_GREEN = 0x00000200,
    IRSDK_FLAG_GREEN_HELD       = 0x00000400,
    IRSDK_FLAG_TEN_TO_GO        = 0x00000800,
    IRSDK_FLAG_FIVE_TO_GO       = 0x00001000,
    IRSDK_FLAG_RANDOM_WAVING    = 0x00002000,
    IRSDK_FLAG_CAUTION          = 0x00004000,
    IRSDK_FLAG_CAUTION_WAVING   = 0x00008000,

    /* Driver black flags */
    IRSDK_FLAG_BLACK            = 0x00010000,
    IRSDK_FLAG_DISQUALIFY       = 0x00020000,
    IRSDK_FLAG_SERVICIBLE       = 0x00040000,
    IRSDK_FLAG_FURLED           = 0x00080000,
    IRSDK_FLAG_REPAIR           = 0x00100000,
    IRSDK_FLAG_DQ_SCORING_INV   = 0x00200000,

    /* Start lights */
    IRSDK_FLAG_START_HIDDEN     = 0x10000000,
    IRSDK_FLAG_START_READY      = 0x20000000,
    IRSDK_FLAG_START_SET        = 0x40000000,
    IRSDK_FLAG_START_GO         = 0x80000000
} irsdk_Flags;

/* Pit service flags (bitfield) */
typedef enum {
    IRSDK_PITSV_LF_TIRE_CHANGE  = 0x0001,
    IRSDK_PITSV_RF_TIRE_CHANGE  = 0x0002,
    IRSDK_PITSV_LR_TIRE_CHANGE  = 0x0004,
    IRSDK_PITSV_RR_TIRE_CHANGE  = 0x0008,
    IRSDK_PITSV_FUEL_FILL       = 0x0010,
    IRSDK_PITSV_WINDSHIELD_TEAROFF = 0x0020,
    IRSDK_PITSV_FAST_REPAIR     = 0x0040
} irsdk_PitSvFlags;

/* Broadcast message types */
typedef enum {
    IRSDK_BROADCAST_CAM_SWITCH_POS = 0,
    IRSDK_BROADCAST_CAM_SWITCH_NUM,
    IRSDK_BROADCAST_CAM_SET_STATE,
    IRSDK_BROADCAST_REPLAY_SET_PLAY_SPEED,
    IRSDK_BROADCAST_REPLAY_SET_PLAY_POSITION,
    IRSDK_BROADCAST_REPLAY_SEARCH,
    IRSDK_BROADCAST_REPLAY_SET_STATE,
    IRSDK_BROADCAST_RELOAD_TEXTURES,
    IRSDK_BROADCAST_CHAT_COMMAND,
    IRSDK_BROADCAST_PIT_COMMAND,
    IRSDK_BROADCAST_TELEM_COMMAND,
    IRSDK_BROADCAST_FFB_COMMAND,
    IRSDK_BROADCAST_REPLAY_SEARCH_SESSION_TIME,
    IRSDK_BROADCAST_VIDEO_CAPTURE,
    IRSDK_BROADCAST_LAST
} irsdk_BroadcastMsg;

/* Pit command modes */
typedef enum {
    IRSDK_PIT_CMD_CLEAR = 0,
    IRSDK_PIT_CMD_WS,
    IRSDK_PIT_CMD_FUEL,
    IRSDK_PIT_CMD_LF,
    IRSDK_PIT_CMD_RF,
    IRSDK_PIT_CMD_LR,
    IRSDK_PIT_CMD_RR,
    IRSDK_PIT_CMD_CLEAR_TIRES,
    IRSDK_PIT_CMD_FR,
    IRSDK_PIT_CMD_CLEAR_WS,
    IRSDK_PIT_CMD_CLEAR_FR,
    IRSDK_PIT_CMD_CLEAR_FUEL,
    IRSDK_PIT_CMD_TC
} irsdk_PitCommandMode;

/* Telemetry command modes */
typedef enum {
    IRSDK_TELEM_CMD_STOP = 0,
    IRSDK_TELEM_CMD_START,
    IRSDK_TELEM_CMD_RESTART
} irsdk_TelemCommandMode;

/* Chat command modes */
typedef enum {
    IRSDK_CHAT_CMD_MACRO = 0,
    IRSDK_CHAT_CMD_BEGIN_CHAT,
    IRSDK_CHAT_CMD_REPLY,
    IRSDK_CHAT_CMD_CANCEL
} irsdk_ChatCommandMode;

/*
 * Data structures
 */

/* Variable header - describes a telemetry variable */
typedef struct {
    int type;           /* irsdk_VarType */
    int offset;         /* Offset from start of buffer row */
    int count;          /* Number of entries (array size) */
    bool count_as_time; /* 1 byte */
    char pad[3];        /* 3 bytes padding for 16-byte alignment */

    char name[IRSDK_MAX_STRING];
    char desc[IRSDK_MAX_DESC];
    char unit[IRSDK_MAX_STRING];
} irsdk_VarHeader;

/* Variable buffer info */
typedef struct {
    int tick_count;     /* Used to detect changes in data */
    int buf_offset;     /* Offset from header */
    int pad[2];         /* Padding for 16-byte alignment */
} irsdk_VarBuf;

/* Main header structure */
typedef struct {
    int ver;                    /* API header version (IRSDK_VER) */
    int status;                 /* Bitfield using irsdk_StatusField */
    int tick_rate;              /* Ticks per second (60 or 360) */

    /* Session information (YAML string) */
    int session_info_update;    /* Incremented when session info changes */
    int session_info_len;       /* Length in bytes of session info string */
    int session_info_offset;    /* Offset to session info string */

    /* Variable data */
    int num_vars;               /* Number of variables */
    int var_header_offset;      /* Offset to irsdk_VarHeader array */

    int num_buf;                /* Number of buffers (<= IRSDK_MAX_BUFS) */
    int buf_len;                /* Length in bytes for one data line */
    int pad[2];                 /* Padding for 16-byte alignment */

    irsdk_VarBuf var_buf[IRSDK_MAX_BUFS];   /* Data buffers */
} irsdk_Header;

/* Disk sub-header (for IBT files) */
typedef struct {
    time_t session_start_date;
    double session_start_time;
    double session_end_time;
    int session_lap_count;
    int session_record_count;
} irsdk_DiskSubHeader;

#endif /* IRA_IRSDK_DEFINES_H */
