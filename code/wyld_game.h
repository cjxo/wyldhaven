/* date = April 7th 2024 0:29 pm */

#ifndef WYLD_GAME_H
#define WYLD_GAME_H

// TODO(christian): decide if we need to us Sprite_TYpe instead of Dungeon_Tile_Type
typedef u32 Sprite_Type;
enum {
    SpriteType_WallFront,
    SpriteType_WallTop,
    SpriteType_WallLeft,
    SpriteType_WallTopLeft,
    SpriteType_WallPillarFront,
    SpriteType_Floor,
};

typedef u8 Dungeon_NodeFlag;
enum {
    DunNodeFlag_IsRoom = 1 << 0,
    DunNodeFlag_IsVerticalSplitChildren = 1 << 1,
};

// NOTE(christian): The room.
typedef struct Dungeon_Node Dungeon_Node;
struct Dungeon_Node {
    RectS32 node_rect;
    RectS32 room_rect;
    Dungeon_NodeFlag node_flag;
    
    Dungeon_Node *left;
    Dungeon_Node *right;
};

typedef struct {
    Dungeon_Node *root;
} Dungeon_GenerateState;

typedef u16 Dungeon_Tile_Flag;
enum {
    DunTileFlag_Walkable = 1 << 0,
    
    // NOTE(christian): distinctions!
    // if DunTileFlag_IlluminatedByPlayer, then the monster can see the player and vice versa!
    // we do not need to test "for each monster, does it see the player?"
    // instead, for all monsters on the tile, they see the player!
    DunTileFlag_Illuminated = 1 << 1,
    DunTileFlag_IlluminatedByPlayer = 1 << 2,
    DunTileFlag_IsNotDiscoveredByPlayer = 1 << 3,
};

typedef u16 Dungeon_Tile_Type;
enum {
    DunTile_WallFront,
    DunTile_WallTop,
    DunTile_WallLeft,
    DunTile_WallTopLeft,
    DunTile_WallPillarFront,
    DunTile_Floor,
    DunTile_StairDown,
    DunTile_StairUp,
};

typedef struct {
    Dungeon_Tile_Flag flags;
    Dungeon_Tile_Type type;
} Dungeon_Tile;

typedef u8 Stat_Type;
enum {
    StatType_Strength,
    StatType_Magic,
    StatType_Dexterity,
    StatType_Agility,
    StatType_Vitality,
    StatType_Luck,
    StatType_Count,
};

typedef u8 Attribute_Type;
enum {
    AttributeType_MaxHP,
    AttributeType_HealPerSecond,
    AttributeType_PhysicalDamage,
    AttributeType_SpecialDamage,
    AttributeType_PhysicalDefence,
    AttributeType_SpecialDefence,
    AttributeType_Accuracy,
    AttributeType_Count,
};

typedef u16 Entity_Type;
enum {
    EntityType_Player,
    EntityType_Count,
};

typedef struct {
    union {
        f32 value_f32;
        u32 value_u32;
    };
} Attribute;

typedef struct {
    u32 stat_pts_to_spend;
    u32 current_stat_pts_per_type[StatType_Count];
    Attribute attributes[AttributeType_Count];
    
    Entity_Type type;
    v2f p;


} Entity;

typedef struct {
	  v2f base_dims;
    
    v2f p;
    v2f dims;
    v2f base_scale;
    v2f scale;
    Entity *subject;
} Camera2D;

typedef struct {
    Dungeon_Tile *tiles;
    s32 width;
    s32 height;
    s32 stair_up_index; // -1 if has no down or up staircase
    s32 stair_down_index; // -1 if has no down or up staircase
} Game_Level;

#define max_level_depth 14
typedef struct {
    Memory_Arena *main_arena;
    UI_State ui_state;
    Work_Queue work_queue;
    R2D_Buffer r2d_buffer;
    
    b32 has_initialized;
    
    R2D_Handle sprite_sheet;
    Game_Level levels[max_level_depth];
    u32 player_current_level;
    b32 climb_up_or_down_debounce;
    
    Entity player;
    Camera2D camera;
    
    u32 tile_dims;
    
    // NOTE(christian): UI stuff
    b32 open_attrib_upgrade;
    UI_Interact stat_interact;
    f32 stat_pts_t;
    f32 stat_pts_t_dir;
    
#if defined(WC_DEBUG)
    s32 subdivs;
    String_U8 subdivs_depth_str;
    u64 dun_gen_cursor;
    b32 debug_enabled_tile_occulution;
#endif
} Game_State;

#endif //WYLD_GAME_H
