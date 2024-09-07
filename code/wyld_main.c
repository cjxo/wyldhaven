#include "wyld_base.h"
#include "wyld_math.h"
#include "os/wyld_os.h"
#include "render/wyld_render.h"
#include "ui/ui.h"
#include "wyld_util.h"
#include "audio/wyld_audio.h"

#include "wyld_base.c"
#include "wyld_math.c"
#include "os/wyld_os.c"
#include "render/wyld_render.c"
#include "ui/ui.c"
#include "wyld_util.c"
#include "audio/wyld_audio.c"

#include <stdio.h>
#include <time.h>

#include "wyld_game.h"

fun Dungeon_Node *
dun_generate_areas(Memory_Arena *arena, RectS32 node_rect, s32 depth) {
  Dungeon_Node *root = arena_push_struct(arena, Dungeon_Node);
  assert_true(root != null);
  root->left = root->right = null;
  root->node_rect = node_rect;
  //root->room_rect = {};
  root->node_flag = 0;
  
  loc f32 min_width_height_ratio = 0.4f;
  if (depth > 0) {
    RectS32 left_container, right_container;
    for (;;) {
      b32 is_vertical_split = rnd_next_normal_f32() < 0.5f;
      
      if (is_vertical_split) {
        left_container.p = node_rect.p;
        left_container.dims = v2i_make(rnd_next_range_u32(1, node_rect.dims.width), node_rect.dims.height);
        
        right_container.p = v2i_make(node_rect.p.x + left_container.dims.width, node_rect.p.y);
        right_container.dims = v2i_make(node_rect.dims.width - left_container.dims.width, node_rect.dims.height);
        
        f32 left_ratio = (f32)(left_container.dims.width) / (f32)(left_container.dims.height);
        f32 right_ratio = (f32)(right_container.dims.width) / (f32)(right_container.dims.height);
        if (!((left_ratio < min_width_height_ratio) || (right_ratio < min_width_height_ratio))) {
          root->node_flag |= DunNodeFlag_IsVerticalSplitChildren;
          break;
        }
      }
      else {
        left_container.p = node_rect.p;
        left_container.dims = v2i_make(node_rect.dims.width, rnd_next_range_u32(1, node_rect.dims.height));
        
        right_container.p = v2i_make(node_rect.p.x, node_rect.p.y + left_container.dims.height);
        right_container.dims = v2i_make(node_rect.dims.width, node_rect.dims.height - left_container.dims.height);
        
        f32 left_ratio = (f32)(left_container.dims.height) / (f32)(left_container.dims.width);
        f32 right_ratio = (f32)(right_container.dims.height) / (f32)(right_container.dims.width);
        if (!((left_ratio < min_width_height_ratio) || (right_ratio < min_width_height_ratio))) {
          break;
        }
      }
    }
    
    root->left = dun_generate_areas(arena, left_container, depth - 1);
    root->right = dun_generate_areas(arena, right_container, depth - 1);
  }
  else
  {
#if 0
    RectS32 *room_rect = &(root->room_rect);
    room_rect->p.x = root->node_rect.p.x + PCG32_Range(0, node_rect.dims.width / 3);
    room_rect->p.y = root->node_rect.p.y + PCG32_Range(0, node_rect.dims.height / 3);
    room_rect->dims.width = root->node_rect.dims.width - (room_rect->p.x - root->node_rect.p.x);
    room_rect->dims.height = root->node_rect.dims.height - (room_rect->p.y - root->node_rect.p.y);
    
    room_rect->dims.width -= PCG32_Range(0, room_rect->dims.width / 3);
    room_rect->dims.height -= PCG32_Range(0, room_rect->dims.height / 3);
#else
    root->room_rect = root->node_rect;
#endif
    root->node_flag |= DunNodeFlag_IsRoom;
  }
  
  return root;
}

inl b32
dun_node_is_leaf(Dungeon_Node *node) {
  b32 result = node && ((node->left == null) && (node->right == null));
  return(result);
}

fun void
dun_generate_walls(Dungeon_Node *root, Dungeon_Tile *tiles, u32 col_count, u32 depth) {
  if (root) {
    dun_generate_walls(root->left, tiles, col_count, depth + 1);
    dun_generate_walls(root->right, tiles, col_count, depth + 1);
    
    if (root->node_flag & DunNodeFlag_IsRoom) {
      s32 max_x = root->room_rect.p.x + root->room_rect.dims.width;
      s32 max_y = root->room_rect.p.y + root->room_rect.dims.height;
      // NOTE(christian): remove double walls
      root->room_rect.p.x = root->room_rect.p.x > 0 ? root->room_rect.p.x - 1 : root->room_rect.p.x;
      root->room_rect.p.y = root->room_rect.p.y > 0 ? root->room_rect.p.y - 1 : root->room_rect.p.y;
      
      for (s32 tile_col = root->room_rect.p.x; tile_col < max_x; tile_col += 1) {
        s32 index = (max_y - 1) * col_count + tile_col;
        tiles[index].flags &= ~DunTileFlag_Walkable;
        tiles[index].type = DunTile_WallTop;
      }
      
      for (s32 tile_row = root->room_rect.p.y; tile_row < max_y; tile_row += 1) {
        s32 index = tile_row * col_count + (max_x - 1);
        tiles[index].flags &= ~DunTileFlag_Walkable;
        tiles[index].type = DunTile_WallLeft;
      }
      
      for (s32 tile_row = root->room_rect.p.y; tile_row < max_y; tile_row += 1) {
        s32 index = tile_row * col_count + root->room_rect.p.x;
        tiles[index].flags &= ~DunTileFlag_Walkable;
        tiles[index].type = DunTile_WallLeft;
      }
      
      for (s32 tile_col = root->room_rect.p.x; tile_col < max_x; tile_col += 1) {
        s32 index = root->room_rect.p.y * col_count + tile_col;
        tiles[index].flags &= ~DunTileFlag_Walkable;
        tiles[index].type = DunTile_WallTop;
        if ((tile_col == root->room_rect.p.x) || (tile_col == (max_x - 1)))
        {
          tiles[index].type = DunTile_WallTopLeft;
        }
        
        index = (root->room_rect.p.y + 1) * col_count + tile_col;
        tiles[index].flags &= ~DunTileFlag_Walkable;
        tiles[index].type = DunTile_WallFront;
        if ((tile_col == root->room_rect.p.x) || (tile_col == (max_x - 1))) {
          tiles[index].type = DunTile_WallLeft;
        }
      }
    }
  }
}

fun void
dun_generate_vertical_corridors(Dungeon_Node *root, Dungeon_Tile *tiles, u32 col_count, u32 depth) {
  if (!root) return;
  
  dun_generate_vertical_corridors(root->left, tiles, col_count, depth + 1);
  dun_generate_vertical_corridors(root->right, tiles, col_count, depth + 1);
  
  if (root->left && root->right) {
    v2i split_start = root->right->node_rect.p;
    if (root->node_flag & DunNodeFlag_IsVerticalSplitChildren) {
      s32 start_y = rnd_next_range_u32(split_start.y + 1, split_start.y + root->right->node_rect.dims.height - 1);
      u32 loop_count_to_avoid_infinite_loop_hack = 0;
      while ((tiles[start_y * col_count + (split_start.x)].type != DunTile_Floor) ||
             (tiles[start_y * col_count + (split_start.x - 2)].type != DunTile_Floor)) {
        start_y = rnd_next_range_u32(split_start.y + 1, split_start.y + root->right->node_rect.dims.height - 1);
        if (++loop_count_to_avoid_infinite_loop_hack > 30) break;
      }
      
      s32 index = start_y * col_count + (split_start.x - 1);
      tiles[index].type = DunTile_Floor;
      tiles[index].flags |= DunTileFlag_Walkable;
      
      index = (start_y - 1) * col_count + (split_start.x - 1);
      tiles[index].type = DunTile_WallPillarFront;
      tiles[index].flags &= ~DunTileFlag_Walkable;
      
      index = (start_y + 1) * col_count + (split_start.x - 1);
      tiles[index].type = DunTile_WallTopLeft;
      tiles[index].flags &= ~DunTileFlag_Walkable;
    }
  }
}

fun void
dun_generate_horizontal_corridors(Dungeon_Node *root, Dungeon_Tile *tiles, u32 col_count, u32 depth) {
  if (!root) return;
  
  dun_generate_horizontal_corridors(root->left, tiles, col_count, depth + 1);
  dun_generate_horizontal_corridors(root->right, tiles, col_count, depth + 1);
  
  if (root->left && root->right) {
    v2i split_start = root->right->node_rect.p;
    if (!(root->node_flag & DunNodeFlag_IsVerticalSplitChildren)) {
      s32 start_x = 0;
      start_x = rnd_next_range_u32(split_start.x + 1, split_start.x + root->right->node_rect.dims.width - 1);
      u32 loop_count_to_avoid_infinite_loop_hack = 0;
      while ((tiles[(split_start.y - 2) * col_count + start_x].type != DunTile_Floor) ||
             (tiles[(split_start.y + 1) * col_count + start_x].type != DunTile_Floor)) {
        start_x = rnd_next_range_u32(split_start.x + 1, split_start.x + root->right->node_rect.dims.width - 1);
        if (++loop_count_to_avoid_infinite_loop_hack > 30) break;
      }
      
      s32 index;
      index = (split_start.y - 1) * col_count + start_x;
      tiles[index].type = DunTile_Floor;
      tiles[index].flags |= DunTileFlag_Walkable;
      index = (split_start.y - 1) * col_count + start_x - 1;
      tiles[index].type = DunTile_WallTopLeft;
      tiles[index].flags &= ~DunTileFlag_Walkable;
      
      index = (split_start.y - 1) * col_count + start_x + 1;
      tiles[index].type = DunTile_WallTopLeft;
      tiles[index].flags &= ~DunTileFlag_Walkable;
      
      index = split_start.y * col_count + start_x;
      tiles[index].type = DunTile_Floor;
      tiles[index].flags |= DunTileFlag_Walkable;
      
      index = split_start.y * col_count + start_x - 1;
      if (tiles[(split_start.y + 1) * col_count + start_x - 1].type == DunTile_Floor)
      {
        tiles[index].type = DunTile_WallPillarFront;
        tiles[index].flags &= ~DunTileFlag_Walkable;
      }
      
      index = split_start.y * col_count + start_x + 1;
      if (tiles[(split_start.y + 1) * col_count + start_x + 1].type == DunTile_Floor)
      {
        tiles[index].type = DunTile_WallPillarFront;
        tiles[index].flags &= ~DunTileFlag_Walkable;
      }
    }
  }
}

fun void
dun_generate(Memory_Arena *arena, Dungeon_Tile *tiles, u32 col_count, u32 row_count, s32 depth) {
  Dungeon_GenerateState state;
  Temporary_Memory temp = temp_mem_begin(arena);
  state.root = dun_generate_areas(arena, r32_make(v2i_make(0, 0), v2i_make(col_count, row_count)), depth);
  
  u32 total_tiles = col_count * row_count;
  
  for (u32 tile_index = 0; tile_index < total_tiles; tile_index += 1) {
    tiles[tile_index].flags = DunTileFlag_Walkable;
    tiles[tile_index].type = DunTile_Floor;
  }
  
  dun_generate_walls(state.root, tiles, col_count, 0);
  
  // NOTE(christian): this is probably dumb for finding a free square for stair
  // plus, the stair could be infront of the door, thus the player couldn't 
  // get into room! But, we could go to corridor creation to avoid this!
  Memory_Arena *find_stair_memory = arena_get_scratch(&arena, 1);
  Temporary_Memory temp_stair_memory = temp_mem_begin(find_stair_memory);
  u32 index_candidate_count = 0;
  u32 *index_candidates = arena_push_array(find_stair_memory, u32, total_tiles);
  for (u32 tile_index = 0; tile_index < total_tiles; tile_index += 1) {
    Dungeon_Tile *tile = tiles + tile_index;
    if (tile->flags & DunTileFlag_Walkable) {
      index_candidates[index_candidate_count++] = tile_index;
    }
    
    tile->flags |= DunTileFlag_IsNotDiscoveredByPlayer;
  }
  
  assert_true(index_candidate_count > 0);
  u32 chosen_index = rnd_next_range_u32(0, index_candidate_count);
  Dungeon_Tile *tile = tiles + index_candidates[chosen_index];
  tile->type = DunTile_StairDown;
  
  temp_mem_end(temp_stair_memory);
  
  dun_generate_vertical_corridors(state.root, tiles, col_count, 0);
  // NOTE(christian): I think this is fixed now
  dun_generate_horizontal_corridors(state.root, tiles, col_count, 0);
  
  temp_mem_end(temp);
}

inl Camera2D
cam2d_make(v2f base_dims, v2f base_scale, Entity *subject) {
  Camera2D result;
  result.base_dims = base_dims;
  result.p = v2f_make(0.0f, 0.0f);
  result.dims = base_dims;
  result.subject = subject;
  result.base_scale = base_scale;
  result.scale = base_scale;
  
  return(result);
}

inl void
cam2d_resize(Camera2D *camera, v2f new_size) {
  f32 ratio_x = (new_size.width / camera->base_dims.width) * camera->base_scale.x;
  f32 ratio_y = (new_size.height / camera->base_dims.height) * camera->base_scale.y;
  camera->dims = new_size;
  
  camera->scale = v2f_make(ratio_x, ratio_y);
}

inl v2f
cam2d_world_to_screen(Camera2D *camera, v2f world_space_p) {
  assert_true(camera->subject != null);
  v2f screen_space_p = v2f_sub(world_space_p, camera->p);
  screen_space_p.x *= camera->scale.x;
  screen_space_p.y *= camera->scale.y;
  return(screen_space_p);
}

inl v2f
cam2d_screen_to_World(Camera2D *camera, v2f screen_space_p) {
  assert_true(camera->subject != null);
  screen_space_p.x /= camera->scale.x;
  screen_space_p.y /= camera->scale.y;
  v2f world_space_p = v2f_add(screen_space_p, camera->p);
  return(world_space_p);
}

inl void
cam2d_update(Camera2D *camera, OS_Input *input, v2f level_dims_pix) {
  if (os_key_released(input, OSInputKey_Q)) {
    v2f_mul_equals(&camera->scale, 2.0f);
  }
  
  if (os_key_released(input, OSInputKey_E)) {
    v2f_mul_equals(&camera->scale, 0.5f);
  }
  
  v2f camera_dim = v2f_make(camera->dims.x / camera->scale.x,
                            camera->dims.y / camera->scale.y);
  
  v2f camera_dim_half = v2f_scale(camera_dim, 0.5f);
  camera->p = v2f_lerp(camera->p, v2f_sub(camera->subject->p, camera_dim_half), 0.4f);
  //camera->p = camera->subject->p - camera_dim_half;
  
  if (camera->p.x < 0) {
    camera->p.x = 0;
  }
  
  if (camera->p.y < 0) {
    camera->p.y = 0;
  }
  
  v2f level_dims_cam_dim_diff = v2f_sub(level_dims_pix, camera_dim);
  if (camera->p.x > level_dims_cam_dim_diff.x) {
    camera->p.x = level_dims_cam_dim_diff.x;
  }
  
  if (camera->p.y > level_dims_cam_dim_diff.y) {
    camera->p.y = level_dims_cam_dim_diff.y;
  }
}

// TODO(christian): decide if we need to use Sprite_TYpe instead of Dungeon_Tile_Type
// But I dont think I need to ... because to 
fun void
draw_dun_tile(Game_State *game_state, R_RenderPass *game_pass, v2f p, v2f dims, Dungeon_Tile *tile) {
  v2f clip_p = { 0 };
  v2f clip_dim = v2f_make(16, 16);
  
  switch (tile->type) {
    case DunTile_WallFront: {
      clip_p = v2f_make(32, 16);
    } break;
    
    case DunTile_WallTop: {
      clip_p = v2f_make(32, 0);
    } break;
    
    case DunTile_WallLeft: {
      clip_p = v2f_make(16, 16);
    } break;
    
    case DunTile_WallTopLeft: {
      clip_p = v2f_make(16, 0);
    } break;
    
    case DunTile_WallPillarFront: {
      clip_p = v2f_make(0, 0);
    } break;
    
    case DunTile_Floor: {
      clip_p = v2f_make(32, 32);
    } break;
    
    case DunTile_StairDown: {
      clip_p = v2f_make(48, 0);
    } break;
    
    case DunTile_StairUp: {
      clip_p = v2f_make(48, 0);
    } break;
    
    default: {
      wc_assert(false && "invalid tile id");
    } break;
  }
  
  R_O2D_Rect *rect = r_O2D_texture_clipped(game_pass, game_state->sprite_sheet, p, dims, clip_p, clip_dim, rgba_make(1, 1, 1, 1));
  if (tile->flags & DunTileFlag_Illuminated) {
    rect->enable_lighting_for_me = true;
  }
  //r_O2D_rect_filled(game_pass, p, dims, rgba_make(1, 1, 0, 1));
  //draw_sprite(game_state, p, dims, dun_tile_type); // for now it directly maps to Sprite_TYpe;
}

/*
shadow casting notes:
- let # denote opaque cells
- let @ be the light
".............."
"......#......."
".#............"
".............."
".......@......"
".............."
".............."
".............."

Suppose we have a grid of cells. Now the goal of of shadow casting is to divide the fov
by octants:

"..\....|..../."
"...\..#|.../.."
".#..\..|../..."
".....\.|./...."
"-------@------"
"...../.|.\...."
"..../..|..\..."
".../...|...\.."

Note:
- the slant lines have a gradient of one (absolute value)
- the ascii isnt drawn to scale, needless to say.

Instead of casting rays starting from the light along the edges, we cast rays visiting
row by rows or column by column:
"|..../."
"|.../.."
"|../..."
"|./...."
"@------"
*/
fun void
dun_cast_light_octant(Dungeon_Tile *level_tiles, s32 level_width, s32 level_height,
                      v2i emanated_from, s32 radius, s32 depth, f32 start_slope,
                      f32 end_slope, s32 step_multiplier_x, s32 step_multiplier_y,
                      s32 step_multiplier_ix, s32 step_multiplier_iy, Dungeon_Tile_Flag illumination_flag) {
  f32 new_start_slope = 0.0f;
  if (start_slope >= end_slope) {
    s32 radius_sq = radius * radius;
    b32 previous_block_is_opaque = false;
    for (s32 depth_index = depth; depth_index <= radius && !previous_block_is_opaque; depth_index += 1) {
      s32 delta_y = -depth_index;
      for (s32 delta_x = delta_y; delta_x <= 0; delta_x += 1) {
        s32 current_tile_x = emanated_from.x + delta_x * step_multiplier_x + step_multiplier_ix * delta_y;
        s32 current_tile_y = emanated_from.y + delta_y * step_multiplier_y + step_multiplier_iy * delta_x;
        
        f32 left_slope = ((f32)delta_x - 0.5f) / ((f32)delta_y + 0.5f);
        f32 right_slope = ((f32)delta_x + 0.5f) / ((f32)delta_y - 0.5f);
        
        if (!((current_tile_x >= 0) && (current_tile_y >= 0) &&
              (current_tile_x < level_width) && (current_tile_y < level_height)) ||
            (start_slope < right_slope)) {
          continue;
        }
        else if (end_slope > left_slope) {
          break;
        }
        
        Dungeon_Tile *current_tile = level_tiles + (current_tile_y * level_width + current_tile_x);
        if (((delta_x * delta_x) + (delta_y * delta_y)) <= radius_sq) {
          current_tile->flags |= illumination_flag;
          current_tile->flags &= ~DunTileFlag_IsNotDiscoveredByPlayer;
          
          //(1.0f / radius_sq)
        }
        
        if (previous_block_is_opaque) {
          if ((current_tile->flags & DunTileFlag_Walkable) == 0) {
            new_start_slope = right_slope;
            continue;
          }
          else {
            previous_block_is_opaque = false;
            start_slope = new_start_slope;
          }
        }
        else if (((current_tile->flags & DunTileFlag_Walkable) == 0) && (depth_index < radius)) {
          previous_block_is_opaque = true;
          dun_cast_light_octant(level_tiles, level_width, level_height,
                                emanated_from, radius, depth_index + 1, start_slope,
                                left_slope, step_multiplier_x, step_multiplier_y,
                                step_multiplier_ix, step_multiplier_iy, illumination_flag);
          new_start_slope = right_slope;
        }
      }
    }
  }
}

fun void
dun_cast_light(Dungeon_Tile *level_tiles, u32 level_width, u32 level_height,
               u32 tile_dims, v2f emanated_from, s32 radius, b32 is_player) {
  read_only s32 quadrant_steps[] = {
    1, 1,
    -1, 1,
    -1, -1,
    1, -1
  };
  
  v2i emanated_from_i = v2f_to_v2i(v2f_inv_scale(emanated_from, (f32)tile_dims));
  level_tiles[emanated_from_i.y * level_width + emanated_from_i.x].flags |= DunTileFlag_Illuminated;
  level_tiles[emanated_from_i.y * level_width + emanated_from_i.x].flags &= ~DunTileFlag_IsNotDiscoveredByPlayer;
  
  Dungeon_Tile_Flag illumination_flag = DunTileFlag_Illuminated;
  if (is_player) {
    illumination_flag |= DunTileFlag_IlluminatedByPlayer;
  }
  
  for (u32 quadrant_index = 0;
       quadrant_index < array_count(quadrant_steps) / 2 + 1;
       quadrant_index += 1) {
    assert_true((quadrant_index + 1) < array_count(quadrant_steps));
    dun_cast_light_octant(level_tiles, (s32)level_width, (s32)level_height,
                          emanated_from_i, radius, 1, 1.0f, 0.0f,
                          quadrant_steps[quadrant_index], quadrant_steps[quadrant_index + 1],
                          0, 0, illumination_flag);
    
    dun_cast_light_octant(level_tiles, (s32)level_width, (s32)level_height,
                          emanated_from_i, radius, 1, 1.0f, 0.0f, 0, 0,
                          quadrant_steps[quadrant_index], quadrant_steps[quadrant_index + 1],
                          illumination_flag);
  }
}

fun void
game_get_player_attribute_increase_from_stat(Entity *player, Stat_Type stat_type,
                                             Attribute new_attributes[AttributeType_Count]) {
  
  Attribute *attributes = player->attributes;
  switch (stat_type) {
    case StatType_Strength: {
      new_attributes[AttributeType_MaxHP].value_f32 = attributes[AttributeType_MaxHP].value_f32 + 0.4f;
      new_attributes[AttributeType_PhysicalDamage].value_f32 = attributes[AttributeType_PhysicalDamage].value_f32 + 0.07f;
      new_attributes[AttributeType_PhysicalDefence].value_f32 = attributes[AttributeType_PhysicalDefence].value_f32 + 0.05f;
    } break;
    
    case StatType_Magic: {
      new_attributes[AttributeType_SpecialDamage].value_f32 = attributes[AttributeType_SpecialDamage].value_f32 + 0.05f;
      new_attributes[AttributeType_SpecialDefence].value_f32 = attributes[AttributeType_SpecialDefence].value_f32 + 0.04f;
    } break;
    
    case StatType_Dexterity: {
      new_attributes[AttributeType_Accuracy].value_f32 = attributes[AttributeType_Accuracy].value_f32 + 0.05f;
    } break;
    
    case StatType_Agility: {
      
    } break;
    
    case StatType_Vitality: {
      new_attributes[AttributeType_MaxHP].value_f32 = attributes[AttributeType_MaxHP].value_f32 + 0.05f;
      new_attributes[AttributeType_HealPerSecond].value_f32 = attributes[AttributeType_HealPerSecond].value_f32 + 0.01f;
    } break;
    
    case StatType_Luck: {
      new_attributes[AttributeType_Accuracy].value_f32 = attributes[AttributeType_Accuracy].value_f32 + 0.02f;
    } break;
    
    default: {
      invalid_code_path();
    } break;
  }
}

fun u64
ent_evaluate_exp_function(Entity *entity) {
  u64 new_max_exp = 0;
  switch (entity->type) {
    case EntityType_Player: {
      assert_true(entity->max_exp > 0);
      new_max_exp = (5 * (entity->level * entity->level * entity->level)) / 4;
    } break;
    
    default: {
      invalid_code_path();
    } break;
  }
  
  return (new_max_exp);
}

fun void
game_ui_player_info(Game_State *game_state, OS_Input *input) {
  UI_State *state = &(game_state->ui_state);
  Entity *player = &(game_state->player);
  f32 healthbar_width = 300.0f;
  f32 healthbar_height = 15.0f;
  
  f32 exp_width = 200.0f;
  f32 exp_height = 10.0f;
  
  ui_push_offset_y(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.85f));
  ui_push_transparent_background(state);
  ui_push_edge_thickness(state, 0.0f);
  ui_push_vert_layout(state, str8("player-health-exp-attrib-ups"), v2f_make(8.0f, 8.0f), v2f_make(0.0f, 8.0f)); {
    ui_pop_offset_y(state);
    
    ui_push_hori_layout(state, str8("attribute-state-layout"), v2f_make(0.0f, 0.0f), v2f_make(4.0f, 0.0f)); {
      ui_pop_transparent_background(state);
      ui_pop_edge_thickness(state);
      
      // I NEED S SYSTEM FOR WIDGET ANIMATION!!!!!!!!!
      v4f pulsate_from = rgba_from_hsva(316.0f, 70.0f, 30.0f, 1.0f);
      v4f pulsate_to = rgba_from_hsva(325.0f, 60.0f, 47.0f, 1.0f);
      
      v4f stat_pts_background_colour;
      if (!(game_state->stat_interact.hover)) {
        v3f lerped;
        if (game_state->stat_pts_t_dir == 1.0f) {
          lerped = v3f_lerp(pulsate_from.rgb, pulsate_to.rgb, ease_in_quad(game_state->stat_pts_t));
        }
        else {
          lerped = v3f_lerp(pulsate_from.rgb, pulsate_to.rgb, ease_out_quad(game_state->stat_pts_t));
        }
        
        stat_pts_background_colour.xyz = lerped;
        stat_pts_background_colour.w = 1.0f;
        
        game_state->stat_pts_t += state->elapsed_time_secs * game_state->stat_pts_t_dir * 3.0f;
        if (game_state->stat_pts_t > 1.0f) {
          game_state->stat_pts_t_dir = -1.0f;
        }
        else if (game_state->stat_pts_t < 0.0f) {
          game_state->stat_pts_t_dir = 1.0f;
        }
      }
      else if (game_state->stat_interact.held) {
        game_state->stat_pts_t_dir = -1.0f;
        v3f lerped = v3f_lerp(pulsate_from.rgb, pulsate_to.rgb, ease_in_quad(game_state->stat_pts_t));
        game_state->stat_pts_t += state->elapsed_time_secs * game_state->stat_pts_t_dir * 16.0f;
        
        if (game_state->stat_pts_t < 0.0f) {
          game_state->stat_pts_t = 0.0f;
        }
        stat_pts_background_colour.xyz = lerped;
        stat_pts_background_colour.w = 1.0f;
      }
      else {
        f32 speed = 3.0f;
        if (game_state->open_attrib_upgrade) {
          speed = 16.0f;
        }
        
        game_state->stat_pts_t_dir = 1.0f;
        v3f lerped = v3f_lerp(pulsate_from.rgb, pulsate_to.rgb, ease_in_quad(game_state->stat_pts_t));
        game_state->stat_pts_t += state->elapsed_time_secs * game_state->stat_pts_t_dir * speed;
        if (game_state->stat_pts_t > 1.0f) {
          game_state->stat_pts_t = 1.0f;
        }
        stat_pts_background_colour.xyz = lerped;
        stat_pts_background_colour.w = 1.0f;
      }
      
      ui_push_background_colour(state, stat_pts_background_colour);
      ui_push_border_colour(state, rgba_from_hsva(330.0f, 42.0f, 54.0f, 1.0f));
      
      ui_push_background_colour(state, pulsate_from);
      ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 32.0f));
      ui_push_texture(state, ui_make_clipped_texture(game_state->sprite_sheet, v2f_make(16 * 6, 16 * 3), v2f_make(16, 16)));
      if (ui_do_button_tex(state, str8("open-player-quick-profile")).released) {
        game_state->open_attrib_upgrade = !game_state->open_attrib_upgrade;
      }
      ui_pop_texture(state);
      ui_pop_individual_size(state);
      
      ui_pop_background_colour(state);
      
      ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 32.0f));
      ui_push_texture(state, ui_make_clipped_texture(game_state->sprite_sheet, v2f_make(112.0f - 16.0f, 0.0f), v2f_make(16.0f, 16.0f)));
      game_state->stat_interact = ui_do_button_tex(state, str8("open-stat-upgrade-btn"));
      ui_pop_texture(state);
      ui_pop_individual_size(state);
      
      if (game_state->stat_interact.released) {
        //game_state->open_attrib_upgrade = !game_state->open_attrib_upgrade;
      }
      
      ui_pop_border_colour(state);
      ui_pop_background_colour(state);
    } ui_pop_layout(state);
    
    ui_push_edge_thickness(state, 2.0f);
    ui_push_corner_roundness(state, 4.0f);
    
    // NOTE(christian): health bar
    ui_next_border_colour(state, rgba_from_hsva(0, 99.0f, 60.0f, 1.0f));
    ui_next_tl_background_colour(state, rgba_from_hsva(0, 90.0f, 40.0f, 1.0f));
    ui_next_bl_background_colour(state, rgba_from_hsva(0, 90.0f, 40.0f, 1.0f));
    ui_next_tr_background_colour(state, rgba_from_hsva(15, 80.0f, 60.0f, 1.0f));
    ui_next_br_background_colour(state, rgba_from_hsva(15, 80.0f, 60.0f, 1.0f));
    ui_do_progression(state, _str8(player - health - bar), UIProgression_Gradient, UIProgressionFlag_ClipBackgroundColour,
                      UIAxis_X, player->health, player->attributes[AttributeType_MaxHP].value_f32,
                      healthbar_width, healthbar_height);
    
    // NOTE(christian): exp bar
    ui_next_border_colour(state, rgba_from_hsva(204, 99.0f, 60.0f, 1.0f));
    ui_next_tl_background_colour(state, rgba_from_hsva(224, 80.0f, 20.0f, 1.0f));
    ui_next_bl_background_colour(state, rgba_from_hsva(224, 80.0f, 20.0f, 1.0f));
    ui_next_tr_background_colour(state, rgba_from_hsva(244, 50.0f, 90.0f, 1.0f));
    ui_next_br_background_colour(state, rgba_from_hsva(244, 50.0f, 90.0f, 1.0f));
    ui_do_progression(state, str8("player-exp-bar"), UIProgression_Gradient, UIProgressionFlag_ClipBackgroundColour,
                      UIAxis_X, (f32)player->exp, (f32)player->max_exp, exp_width, exp_height);
    
    ui_pop_corner_roundness(state);
    ui_pop_edge_thickness(state);
  } ui_pop_layout(state);
  
  // TODO(christian): More UI features:
  // - steal idea: https://www.google.com/search?q=roguelike+stat+attribute+UI+upgrades&sca_esv=bdce78cde9db5882&rlz=1C1ONGR_enPH1088PH1088&udm=2&biw=1536&bih=695&ei=WNUpZvKUIe6RseMPh9-7kAE&ved=0ahUKEwjyoJmdvdyFAxXuSGwGHYfvDhIQ4dUDCBA&uact=5&oq=roguelike+stat+attribute+UI+upgrades&gs_lp=Egxnd3Mtd2l6LXNlcnAiJHJvZ3VlbGlrZSBzdGF0IGF0dHJpYnV0ZSBVSSB1cGdyYWRlc0jjHVAAWJgdcAB4AJABAZgBggKgAcISqgEGMC4xNi4yuAEDyAEA-AEBmAIAoAIAmAMAkgcAoAeqBg&sclient=gws-wiz-serp#vhid=zXt7mW6vY066AM&vssid=mosaic
  // - steal idea: https://www.google.com/search?sca_esv=8d63fbbca7a20b91&udm=2&q=physical+defence+pixel+art&spell=1&sa=X&ved=2ahUKEwiml-XvtfOFAxUvbmwGHd-rAjEQBSgAegQIBxAC&biw=1536&bih=703&dpr=1.25#vhid=yyhA0tPMEMtJ_M&vssid=mosaic
  if (game_state->open_attrib_upgrade) {
    //v4f text_colour = rgba_from_hsva(355.0f, 20.0f, 90.0f, 1.0);
    //v4f text_colour = rgba_from_hsva(68.0f, 36.0f, 52.0f, 1.0);
    v4f attribute_upgrades_bg_col = rgba_from_hsva(330.0f, 47.0f, 26.0f, 1.0);
    v4f attribute_upgrades_border_col = rgba_from_hsva(10.0f, 25.0f, 80.0f, 1.0);
    
    ui_next_background_colour(state, attribute_upgrades_bg_col);
    ui_next_border_colour(state, attribute_upgrades_border_col);
    ui_next_offset(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.5f));
    ui_next_anchor(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
    ui_push_vert_layout(state, str8("player-attrib-box"), v2f_make(8.0f, 8.0f), v2f_make(0.0f, 4.0f)); {
      ui_next_transparent_background(state);
      ui_next_transparent_border(state);
      ui_next_offset_x(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Percentage, 0.5f));
      ui_next_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
      ui_next_font_size(state, R_FontSize_Medium);
      ui_do_label(state, str8("(Player Class) Player Name"));
      
      ui_push_edge_thickness(state, 0.0f);
      ui_next_transparent_background(state);
      ui_push_hori_layout(state, str8("player-attrib-upgrades-container"), v2f_make(0.0f, 0.0f), v2f_make(4.0f, 0.0f)); {
        ui_next_tl_background_colour(state, attribute_upgrades_bg_col);
        ui_next_tr_background_colour(state, attribute_upgrades_bg_col);
        ui_next_bl_background_colour(state, rgba_from_hsva(330.0f, 35.0f, 48.0f, 1.0f));
        ui_next_br_background_colour(state, rgba_from_hsva(330.0f, 35.0f, 48.0f, 1.0f));
        ui_push_vert_layout(state, str8("player-details-stuff"), v2f_make(12.0f, 12.0f), v2f_make(0.0f, 0.0f)); {
          ui_pop_edge_thickness(state);
          
          ui_push_transparent_background(state);
          ui_push_transparent_border(state);
          
          ui_do_labelf(state, str8("Level %llu"), player->level);
          ui_do_labelf(state, str8("Health: %.2f/%.2f"), player->health, player->attributes[AttributeType_MaxHP].value_f32);
          ui_do_labelf(state, str8("EXP: %llu/%llu"), player->exp, player->max_exp);
          ui_do_labelf(state, str8("Mana: %u/%u"), player->mana, 100);
          ui_do_labelf(state, str8("Energy: %.2f/%.2f"), player->energy, 100.0f);
          
          ui_pop_transparent_border(state);
          ui_pop_transparent_background(state);
        } ui_pop_layout(state);
        
        Attribute new_attributes[AttributeType_Count] = { 0 };
        
        ui_push_tl_background_colour(state, attribute_upgrades_bg_col);
        ui_push_tr_background_colour(state, attribute_upgrades_bg_col);
        ui_push_bl_background_colour(state, rgba_from_hsva(330.0f, 35.0f, 48.0f, 1.0f));
        ui_push_br_background_colour(state, rgba_from_hsva(330.0f, 35.0f, 48.0f, 1.0f));
        
        ui_push_edge_thickness(state, 0.0f);
#if 0
        ui_push_vert_layout(state, str8("player-attrib-upgrades"), v2f_make(12.0f, 12.0f), v2f_make(0.0f, 8.0f)); {
          ui_pop_background_colour(state);
          ui_pop_edge_thickness(state);
          
          //ui_push_border_colour(state, attribute_upgrades_border_col);
          
          ui_push_transparent_background(state);
          //ui_pop_transparent_border(state);
          ui_push_edge_thickness(state, 0.0f);
          ui_push_text_colour(state, text_colour);
          ui_do_labelf(state, str8("Stat Points: %u"), player->stat_pts_to_spend);
          ui_pop_text_colour(state);
          
          ui_push_hori_layout(state, str8("player-attrib-state-display"), v2f_make(0.0f, 0.0f), v2f_make(16.0f, 0.0f)); {
            ui_push_anchor_y(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
            ui_push_offset_y(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Percentage, 0.5f));
            ui_push_vert_layout(state, str8("player-attrib-names"), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 7.0f)); {
              ui_pop_anchor_y(state);
              ui_pop_offset_y(state);
              // NOTE(christian): https://www.roguebasin.com/index.php/Attribute
              // TODO: Define how these stats affect our attributes!
              // Strength
              // - A measure of physical force that can be exerted upon other things,
              //   or the ability of a character to perform some physical action.
              ui_do_label(state, str8("Strength"));
              
              ui_do_label(state, str8("Magic"));
              // Dexterity
              // - A measure of physical control, accuracy, or precision in 
              //   relation to certain actions.
              ui_do_label(state, str8("Dexterity"));
              
              // Agility
              // - A measure of speed or the deftness of movement.
              ui_do_label(state, str8("Agility"));
              
              // Vitality
              // - Also "constitution" or "vigor". A measure of health
              //   or the ability to resist physical ailments like poison or illness.
              ui_do_label(state, str8("Vitality"));
              
              // Luck
              // - A measure of how favorable the odds are against the character,
              //   for things such as treasure drops or damage rolls.
              ui_do_label(state, str8("Luck"));
            } ui_pop_layout(state);
            
            //ui_push_edge_thickness(state, 1.0f);
            ui_push_vert_layout(state, str8("player-attrib-values-and-upgrades"), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 4.0f)); {
              Memory_Arena *format_memory = arena_get_scratch(&(game_state->main_arena), 1);
              Temporary_Memory format_temp_memory = temp_mem_begin(format_memory);
              ui_push_texture(state, ui_make_clipped_texture(game_state->sprite_sheet, v2f_make(112.0f, 32.0f), v2f_make(16.0f, 16.0f)));
              for (u32 stat_idx = 0; stat_idx < StatType_Count; ++stat_idx) {
                ui_push_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 1.0f));
                ui_push_offset_x(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Percentage, 1.0f));
                ui_push_hori_layout(state, str8_format(format_memory, str8("PlayerStat%u###Upgrade"), stat_idx), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 0.0f)); {
                  ui_pop_anchor_x(state);
                  ui_pop_offset_x(state);
                  ui_push_anchor_y(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
                  ui_push_offset_y(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Percentage, 0.5f));
                  ui_do_labelf(state, str8("PlayerStat%u###%u"), stat_idx, player->current_stat_pts_per_type[stat_idx]);
                  ui_push_corner_roundness(state, 4.0f);
                  ui_push_background_colour(state, rgba_from_hsva(346.0f, 34.0f, 48.0f, 1.0f));
                  ui_push_border_colour(state, rgba_from_hsva(330.0f, 55.0f, 43.0f, 1.0f));
                  ui_push_edge_thickness(state, 1.0f);
                  //UI_Interact btn_interact = ui_do_buttonf(state, str8("PlayerStat%u###+"), stat_idx);
                  
                  ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 32.0f));
                  UI_Interact btn_interact = ui_do_button_tex(state, str8_format(format_memory, str8("PlayerStatTex%u"), stat_idx));
                  ui_pop_individual_size(state);
                  
                  ui_pop_edge_thickness(state);
                  ui_pop_border_colour(state);
                  ui_pop_background_colour(state);
                  ui_pop_corner_roundness(state);
                  
                  ui_pop_offset_y(state);
                  ui_pop_anchor_y(state);
                  if (btn_interact.hover) {
                    // If we hover, then highlight in green the possible player attribute
                    // improvement(s).
                    game_get_player_attribute_increase_from_stat(&game_state->player,
                                                                 (Stat_Type)stat_idx,
                                                                 new_attributes);
                  }
                  
                  if (player->stat_pts_to_spend && btn_interact.released) {
                    assert_true(btn_interact.hover != 0);
                    player->current_stat_pts_per_type[stat_idx] += 1;
                    player->stat_pts_to_spend -= 1;
                    // TODO: If we released, then finalize the upgrades and reflect the result
                    // below. 
                    //  <> We need some sort of "memory" of whats the next increase of the attributes.
                    //  <> We could to this with some fixed increments or hard coded values.
                    //  <> I'd with fixed increments because the player can have 10000 stat points
                    //     and there is no way I am hard-coding 10000 increase values per stat.
                    //  <> So, under the assumption of fixed increments, the rate depends on the class
                    //     of the player. Well, I haven't defined player classes yet, thus for now,
                    //     we do fixed increments of some arbitrary choice.
                    
                    // APPLY UPGRADES
                    for (u32 attribute_idx = 0; attribute_idx < AttributeType_Count; ++attribute_idx) {
                      if (new_attributes[attribute_idx].value_f32 > 0.0f) {
                        game_state->player.attributes[attribute_idx].value_f32 = new_attributes[attribute_idx].value_f32;
                      }
                    }
                  }
                } ui_pop_layout(state);
              }
              temp_mem_end(format_temp_memory);
              ui_pop_texture(state);
              
            } ui_pop_layout(state);
            ui_pop_edge_thickness(state);
          } ui_pop_layout(state);
          //                    ui_pop_edge_thickness(state);
          ui_pop_transparent_background(state);
        } ui_pop_layout(state);
#endif
        
        ui_next_tl_background_colour(state, attribute_upgrades_bg_col);
        ui_next_tr_background_colour(state, attribute_upgrades_bg_col);
        ui_next_bl_background_colour(state, rgba_from_hsva(330.0f, 35.0f, 48.0f, 1.0f));
        ui_next_br_background_colour(state, rgba_from_hsva(330.0f, 35.0f, 48.0f, 1.0f));
        
        //ui_push_background_colour(state, attribute_upgrades_bg_col);
        ui_next_edge_thickness(state, 0.0f);
        ui_push_vert_layout(state, str8("player-attrib-improvements"), v2f_make(24.0f, 12.0f), v2f_make(0.0f, 8.0f)); {
          //ui_pop_edge_thickness(state);
          //ui_pop_background_colour(state);
          //ui_pop_edge_thickness(state);
          
          ui_push_border_colour(state, attribute_upgrades_border_col);
          ui_push_edge_thickness(state, 1.0f);
          
#if 0
          ui_push_offset_x(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.5f));
          ui_push_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
          ui_do_label(state, str8("Attribute###Attributes"));
          ui_pop_offset_x(state);
          ui_pop_anchor_x(state);
#endif
          
          ui_push_edge_thickness(state, 0.0f);
          ui_push_transparent_background(state);
          ui_push_vert_layout(state, str8("Attribute-Improvements"), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 0.0f)); {
            //-
            Memory_Arena *format_memory = arena_get_scratch(&(game_state->main_arena), 1);
            Temporary_Memory temp_format_memory = temp_mem_begin(format_memory);
            ui_push_corner_roundness(state, 0.0f);
            
            read_only char *
              attribute_display[] = {
              "Max HP: ",
              "Heal rate: ",
              "Phy Dmg: ",
              "Sp Dmg: ",
              "Phy Def: ",
              "Sp Def: ",
              "Acc: ",
            };
            
            read_only v2f
              texture_clip_p[] = {
              { 16.0f * 3.0f, 6.0f * 16.0f },
              { 16.0f * 5.0f, 6.0f * 16.0f },
              { 16.0f * 0.0f, 6.0f * 16.0f },
              { 16.0f * 1.0f, 6.0f * 16.0f },
              { 16.0f * 2.0f, 6.0f * 16.0f },
              { 16.0f * 2.0f, 6.0f * 16.0f },
              { 16.0f * 4.0f, 6.0f * 16.0f },
            };
            
            for (u32 attribute_idx = 0; attribute_idx < AttributeType_Count; ++attribute_idx) {
              ui_push_hori_layout(state, str8_format(format_memory, str8("PlayerAttributeNumber(%d)###IconTextPair"), attribute_idx), v2f_make(0.0f, 0.0f), v2f_make(12.0f, 0.0f)); {
                ui_push_anchor_y(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
                ui_push_offset_y(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Percentage, 0.5f));
                
                ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 16.0f));
                ui_push_texture(state, ui_make_clipped_texture(game_state->sprite_sheet, texture_clip_p[attribute_idx], v2f_make(16.0f, 16.0f)));
                ui_do_label_tex(state, str8_format(format_memory, str8("PlayerAttributeNumber(%d)-Icon"), attribute_idx));
                ui_pop_texture(state);
                ui_pop_individual_size(state);
                
                ui_push_text_padding_x(state, 0.0f);
                ui_push_hori_layout(state, str8_format(format_memory, str8("PlayerAttributeNumber(%d)###Display"), attribute_idx), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 0.0f)); {
                  ui_do_labelf(state, str8("PlayerAttributeNumber(%d)###%s"), attribute_idx, attribute_display[attribute_idx]);
                  if (new_attributes[attribute_idx].value_f32 > 0.0f) {
                    ui_push_text_colour(state, rgba_from_hsva(119.0f, 75.0f, 71.0f, 1.0f));
                    ui_do_labelf(state, str8("PlayerAttributeNumber(%d)###%0.2f"), attribute_idx, new_attributes[attribute_idx].value_f32);
                    ui_pop_text_colour(state);
                  }
                  else {
                    ui_do_labelf(state, str8("PlayerAttributeNumber(%d)###%0.2f"), attribute_idx, game_state->player.attributes[attribute_idx].value_f32);
                  }
                } ui_pop_layout(state);
                ui_pop_text_padding_x(state);
                ui_pop_anchor_y(state);
                ui_pop_offset_y(state);
              } ui_pop_layout(state);
            }
            temp_mem_end(temp_format_memory);
            
            ui_pop_corner_roundness(state);
          } ui_pop_layout(state);
          
          ui_pop_border_colour(state);
          ui_pop_edge_thickness(state);
          ui_pop_transparent_background(state);
        } ui_pop_layout(state);
      } ui_pop_layout(state);
      ui_pop_background_colour(state);
      
      ui_push_offset_x(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Percentage, 0.5f));
      ui_push_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
      ui_push_edge_thickness(state, 0.0f);
      ui_push_hori_layout(state, str8("player-attrib-confirmation"), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 0.0f)); {
        ui_pop_anchor_x(state);
        ui_pop_edge_thickness(state);
        ui_pop_offset_x(state);
        
        ui_push_background_colour(state, attribute_upgrades_bg_col);
        if (ui_do_button(state, str8("AntiDupe###OK")).released) {
          game_state->open_attrib_upgrade = false;
        }
        
        ui_pop_background_colour(state);
      } ui_pop_layout(state);
    } ui_pop_layout(state);
  }
  
  // TODO(christian): design ability bar. There are 6 max slots. I don't know if its a good idea
  // to imitate pokemon: discard existing ability (if the slots are full), or we just store abilities in inventory???
  ui_push_anchor_y(state, ui_make_anchor(UIMetricType_Percentage, 1.0f));
  ui_push_offset_y(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.98f));
  ui_push_offset_x(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.50f));
  ui_push_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
  ui_push_edge_thickness(state, 0.0f);
  ui_push_transparent_background(state);
  ui_push_vert_layout(state, str8("player-abilities-display"), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 0.0f)); {
    ui_pop_edge_thickness(state);
    ui_pop_anchor_y(state);
    ui_pop_anchor_x(state);
    ui_pop_offset_x(state);
    ui_pop_offset_y(state);
    
    ui_push_edge_thickness(state, 0.0f);
    ui_push_hori_layout(state, str8("player-abilities-row0"), v2f_make(0.0f, 0.0f), v2f_make(4.0f, 0.0f)); {
      ui_pop_edge_thickness(state);
      
      ui_push_text_padding_x(state, 16.0f);
      ui_push_text_padding_y(state, 12.0f);
      ui_do_label(state, str8("antidupey0###1"));
      ui_do_label(state, str8("antidupey0###2"));
      ui_do_label(state, str8("antidupey0###3"));
      ui_do_label(state, str8("antidupey0###4"));
      ui_do_label(state, str8("antidupey0###5"));
      ui_do_label(state, str8("antidupey0###6"));
      ui_pop_text_padding(state);
    } ui_pop_layout(state);
    
    ui_pop_transparent_background(state);
  } ui_pop_layout(state);
  
  // NOTE(christian): Inventory. Player can expand inventory.
  ui_next_edge_thickness(state, 0.0f);
  ui_next_offset_x(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 1.0f));
  ui_next_offset_y(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.985f));
  ui_next_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 1.0f));
  ui_next_anchor_y(state, ui_make_anchor(UIMetricType_Percentage, 1.0f));
  ui_push_transparent_background(state);
  ui_push_vert_layout(state, str8("player-weapon-abilities"), v2f_make(8.0f, 0.0f), v2f_make(0.0f, 4.0f)); {
    //ui_pop_anchor_y(state);
    
    ui_next_edge_thickness(state, 0.0f);
    ui_push_hori_layout(state, str8("player-weapon-abilities-row0"), v2f_make(0.0f, 0.0f), v2f_make(6.0f, 0.0f)); {
      
      ui_push_text_padding(state, 0);
      ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 48));
      ui_do_label(state, str8("player-weapon-abilities0###"));
      ui_do_label(state, str8("player-weapon-abilities1###"));
      ui_do_label(state, str8("player-weapon-abilities2###"));
      ui_do_label(state, str8("player-weapon-abilities3###"));
      ui_do_label(state, str8("player-weapon-abilities4###"));
      ui_pop_individual_size(state);
      ui_pop_text_padding(state);
    } ui_pop_layout(state);
    
  } ui_pop_layout(state);
  ui_pop_transparent_background(state);
  
  if (os_key_released(input, OSInputKey_I)) {
    game_state->open_expanded_inventory = !game_state->open_expanded_inventory;
  }
  
  if (game_state->open_expanded_inventory) {
    ui_next_edge_thickness(state, 0.0f);
    ui_next_offset_y(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.5f));
    ui_next_offset_x(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.5f));
    ui_next_anchor(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
    ui_push_vert_layout(state, str8("player-equipment-and-inventory-container"), v2f_make_uniform(0.0f), v2f_make(0.0f, 8.0f)); {
      ui_next_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 32.0f));
      ui_next_texture(state, ui_make_clipped_texture(game_state->sprite_sheet, v2f_make(7.0f * 16.0f, 3.0f * 16.0f), v2f_make(16.0f, 16.0f)));
      ui_next_edge_thickness(state, 1.0f);
      ui_next_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 1.0f));
      ui_next_offset_x(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Percentage, 0.99f));
      ui_do_label_tex(state, str8("player-equipment-and-inventory-close-btn"));
      
      ui_next_edge_thickness(state, 0.0f);
      ui_push_hori_layout(state, str8("player-equipment-and-inventory"), v2f_make(0.0f, 0.0f), v2f_make(12.0f, 0.0f)); {
        ui_next_edge_thickness(state, 1.0f);
        ui_push_vert_layout(state, str8("player-currently-equipped-stuff"), v2f_make(16.0f, 16.0f), v2f_make(0.0f, 8.0f)); {
          ui_next_font_size(state, R_FontSize_Large);
          ui_next_text_padding(state, 0.0f);
          ui_push_edge_thickness(state, 0.0f);
          ui_next_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
          ui_next_offset_x(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.5f));
          ui_do_label(state, str8("player-equipment###Equipment"));
          
          ui_push_drag_and_drop_handle(state, UIDragAndDropFlag_Weapon);
          ui_push_transparent_background(state);
          ui_push_hori_layout(state, str8("main-hand-equip-row"), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 0.0f)); {
            ui_push_text_padding_x(state, 16.0f);
            ui_push_text_padding_y(state, 12.0f);
            ui_do_label(state, str8("main-hand-equip###Main Hand"));
            
            ui_next_corner_roundness(state, 0.0f);
            ui_next_edge_thickness(state, 1.0f);
            ui_next_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 48.0f));
            ui_do_drag_and_drop_tex(state, str8("todo:drag-and-droppable-main-hand###0"));
            ui_pop_text_padding(state);
          } ui_pop_layout(state);
          
          ui_push_hori_layout(state, str8("off-hand-equip-row"), v2f_make(0.0f, 0.0f), v2f_make(0.0f, 0.0f)); {
            ui_push_text_padding_x(state, 16.0f);
            ui_push_text_padding_y(state, 12.0f);
            ui_do_label(state, str8("off-hand-equip###Off Hand"));
            
            ui_next_corner_roundness(state, 0.0f);
            ui_next_edge_thickness(state, 1.0f);
            ui_next_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 48.0f));
            ui_do_drag_and_drop_tex(state, str8("todo:drag-and-droppable-off-hand###1"));
            ui_pop_text_padding(state);
          } ui_pop_layout(state);
          ui_pop_drag_and_drop_handle(state);
          
          ui_pop_transparent_background(state);
        } ui_pop_layout(state);
        
        ui_next_edge_thickness(state, 1.0f);
        ui_push_vert_layout(state, str8("player-inventory-expanded"), v2f_make(16.0f, 16.0f), v2f_make(0.0f, 6.0f)); {
          ui_next_font_size(state, R_FontSize_Large);
          ui_next_text_padding(state, 0.0f);
          ui_push_edge_thickness(state, 0.0f);
          ui_next_anchor_x(state, ui_make_anchor(UIMetricType_Percentage, 0.5f));
          ui_next_offset_x(state, ui_make_offset(UIOffsetType_Absolute, UIMetricType_Percentage, 0.5f));
          ui_do_label(state, str8("player-intentory###Items"));
          
          ui_push_drag_and_drop_handle(state, UIDragAndDropFlag_Weapon|UIDragAndDropFlag_Potion);
          ui_push_hori_layout(state, str8("player-inventory-expanded-row0"), v2f_make(0.0f, 0.0f), v2f_make(6.0f, 0.0f)); {
            ui_pop_edge_thickness(state);
            
            ui_push_edge_thickness(state, 1.0f);
            ui_push_corner_roundness(state, 1.0f);
            
            ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 16 * 3));
            
            ui_next_drag_and_drop_item(state,
                                       ui_make_drag_and_drop_item(ui_make_clipped_texture(game_state->sprite_sheet, v2f_make(16.0f * 13.0f, 16.0f * 0.0f), v2f_make(48, 48)),
                                                                  UIDragAndDropFlag_Weapon));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###A"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###B"));
            
            ui_next_drag_and_drop_item(state,
                                       ui_make_drag_and_drop_item(ui_make_clipped_texture(game_state->sprite_sheet, v2f_make(16.0f * 4.0f, 6.0f * 16.0f), v2f_make(16, 16)),
                                                                  UIDragAndDropFlag_Potion));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###C"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###D"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###E"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###F"));
            ui_pop_individual_size(state);
            
            ui_pop_corner_roundness(state);
            ui_pop_edge_thickness(state);
            
          } ui_pop_layout(state);
          
          ui_push_edge_thickness(state, 0.0f);
          ui_push_hori_layout(state, str8("player-inventory-expanded-row1"), v2f_make(0.0f, 0.0f), v2f_make(6.0f, 0.0f)); {
            ui_pop_edge_thickness(state);
            ui_push_edge_thickness(state, 1.0f);
            ui_push_corner_roundness(state, 1.0f);
            
            ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 16 * 3));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###G"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###H"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###I"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###J"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###K"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###L"));
            ui_pop_individual_size(state);
            
            ui_pop_corner_roundness(state);
            ui_pop_edge_thickness(state);
          } ui_pop_layout(state);
          
          ui_push_edge_thickness(state, 0.0f);
          ui_push_hori_layout(state, str8("player-inventory-expanded-row2"), v2f_make(0.0f, 0.0f), v2f_make(6.0f, 0.0f)); {
            ui_pop_edge_thickness(state);
            ui_push_edge_thickness(state, 1.0f);
            ui_push_corner_roundness(state, 1.0f);
            
            ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 16 * 3));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###M"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###N"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###O"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###P"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###Q"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###R"));
            ui_pop_individual_size(state);
            
            ui_pop_corner_roundness(state);
            ui_pop_edge_thickness(state);
          } ui_pop_layout(state);
          
          ui_push_edge_thickness(state, 0.0f);
          ui_push_hori_layout(state, str8("player-inventory-expanded-row3"), v2f_make(0.0f, 0.0f), v2f_make(6.0f, 0.0f)); {
            ui_pop_edge_thickness(state);
            ui_push_edge_thickness(state, 1.0f);
            ui_push_corner_roundness(state, 1.0f);
            
            ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 16 * 3));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###S"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###T"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###U"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###V"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###W"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###X"));
            ui_pop_individual_size(state);
            
            ui_pop_corner_roundness(state);
            ui_pop_edge_thickness(state);
          } ui_pop_layout(state);
          
          ui_push_edge_thickness(state, 0.0f);
          ui_push_hori_layout(state, str8("player-inventory-expanded-row4"), v2f_make(0.0f, 0.0f), v2f_make(6.0f, 0.0f)); {
            ui_pop_edge_thickness(state);
            ui_push_edge_thickness(state, 1.0f);
            ui_push_corner_roundness(state, 1.0f);
            
            ui_push_individual_size(state, ui_make_individual_size(UIIndividualSize_Pixels, 16 * 3));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###Y"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###Z"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###1"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###2"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###3"));
            ui_do_drag_and_drop_tex(state, str8("PlayerInventoryExpanded###4"));
            ui_pop_individual_size(state);
            
            ui_pop_corner_roundness(state);
            ui_pop_edge_thickness(state);
          } ui_pop_layout(state);
          
          ui_pop_drag_and_drop_handle(state);
          
        } ui_pop_layout(state);
        
      } ui_pop_layout(state);
    } ui_pop_layout(state);
  }
}

fun void
game_update_and_render(Game_State *game_state, OS_Input *input,
                       R_Buffer *buffer, f32 dt_step) {
  prof_begin_timed_block();
  R_RenderPass *game_pass = r_begin_pass(buffer, R_RenderPassType_Game_Ortho2D);
  game_pass->game_ortho_2D.enable_lighting = game_state->enable_lights;
  v2i player_request_walk = { 0 };
  if (os_key_pressed(input, OSInputKey_W)) {
    player_request_walk.y -= 32;
  }
  
  if (os_key_pressed(input, OSInputKey_S)) {
    player_request_walk.y += 32;
  }
  
  if (os_key_pressed(input, OSInputKey_D)) {
    player_request_walk.x += 32;
  }
  
  if (os_key_pressed(input, OSInputKey_A)) {
    player_request_walk.x -= 32;
  }
  
  Memory_Arena *main_arena = game_state->main_arena;
  UI_State *ui_state = &(game_state->ui_state);
  Camera2D *camera = &(game_state->camera);
  Entity *player = &(game_state->player);
  Game_Level *level = game_state->levels + game_state->player_current_level;
  
  if (!!player_request_walk.x ^ !!player_request_walk.y) {
    v2i player_p = v2f_to_v2i(player->p);
    v2i new_p = v2i_add(player_p, player_request_walk);
    new_p.x /= game_state->tile_dims;
    new_p.y /= game_state->tile_dims;
    
    if ((new_p.x >= 0) && (new_p.x < (s32)(level->width)) &&
        (new_p.y >= 0) && (new_p.y < (s32)(level->height))) {
      Dungeon_Tile *tile = level->tiles + (new_p.y * level->width + new_p.x);
      if (tile->flags & DunTileFlag_Walkable) {
        v2f_plus_equals(&player->p, v2i_to_v2f(player_request_walk));
        game_state->climb_up_or_down_debounce = false;
      }
    }
  }
  
  if (!game_state->climb_up_or_down_debounce) {
    v2i player_tile_p = v2f_to_v2i(player->p);
    player_tile_p.x /= game_state->tile_dims;
    player_tile_p.y /= game_state->tile_dims;
    if ((player_tile_p.x >= 0) && (player_tile_p.x < (s32)(level->width)) &&
        (player_tile_p.y >= 0) && (player_tile_p.y < (s32)(level->height))) {
      
      Dungeon_Tile *tile = level->tiles + (player_tile_p.y * level->width + player_tile_p.x);
      assert_true(tile != null);
      
      if ((tile->type == DunTile_StairDown) &&
          (game_state->player_current_level < max_level_depth)) {
        game_state->player_current_level += 1;
        game_state->climb_up_or_down_debounce = true;
        
        Game_Level *next_level = game_state->levels + game_state->player_current_level;
        player->p = v2i_to_v2f(v2i_make(next_level->stair_up_index % next_level->width,
                                        next_level->stair_up_index / next_level->width));
        v2f_mul_equals(&player->p, (f32)game_state->tile_dims);
      }
      else if ((tile->type == DunTile_StairUp) &&
               (game_state->player_current_level > 0)) {
        game_state->player_current_level -= 1;
        game_state->climb_up_or_down_debounce = true;
        
        Game_Level *next_level = game_state->levels + game_state->player_current_level;
        player->p = v2i_to_v2f(v2i_make(next_level->stair_down_index % next_level->width,
                                        next_level->stair_down_index / next_level->width));
        v2f_mul_equals(&player->p, (f32)game_state->tile_dims);
      }
    }
  }
  
  if (player->health < player->attributes[AttributeType_MaxHP].value_f32) {
    player->health += player->attributes[AttributeType_HealPerSecond].value_f32 * dt_step;
    
    player->health = minimum(player->health, player->attributes[AttributeType_MaxHP].value_f32);
  }
  
  dun_cast_light(level->tiles, level->width, level->height, game_state->tile_dims, player->p, 8, true);
  
  v2f tile_dims_screen_v = v2f_make(game_state->tile_dims * camera->scale.x, game_state->tile_dims * camera->scale.y);
  v2f map_dims_world_space = v2f_make((f32)level->width * game_state->tile_dims, (f32)level->height * game_state->tile_dims);
  
  cam2d_update(camera, input, map_dims_world_space);
  
  v2i render_begin = v2i_max(v2i_sub(v2f_to_v2i(v2f_inv_scale(cam2d_screen_to_World(camera, v2f_make(0, 0)), (f32)game_state->tile_dims)), v2i_make(1, 1)), v2i_make(0, 0));
  v2i render_end = v2i_min(v2i_add(v2f_to_v2i(v2f_inv_scale(cam2d_screen_to_World(camera, v2f_min(map_dims_world_space, camera->dims)), (f32)game_state->tile_dims)), v2i_make(1, 1)),
                           v2i_make(level->width, level->height));
  
  f32 tile_gap = 0.0f;
  
  //render_begin_x = (viewport_width - map_dims_pix.x) * 0.5f;
  //render_begin_y = (viewport_height - map_dims_pix.y) * 0.5f;
  
  v2f player_screen_p = v2f_add(cam2d_world_to_screen(camera, player->p), v2f_make(16.0f, 16.0f));
  r_O2D_light_add(game_pass, player_screen_p, 8.0f * 32.0f, v4f_make(1.0f, 1.0f, 1.0f, 1.0f));
  
  for (s32 tile_y = render_begin.y; tile_y < render_end.y; tile_y += 1) {
    for (s32 tile_x = render_begin.x; tile_x < (render_end.x); tile_x += 1) {
      s32 index = tile_y * level->width + tile_x;
      Dungeon_Tile *tile = level->tiles + index;
      if (!game_state->debug_enabled_tile_occulution || ((tile->flags & DunTileFlag_IsNotDiscoveredByPlayer) == 0)) {
        v2f p = v2f_make((f32)tile_x * game_state->tile_dims, (f32)tile_y * game_state->tile_dims);
        p.x += tile_x * tile_gap;
        p.y += tile_y * tile_gap;
        
        p = cam2d_world_to_screen(camera, p);
        
        draw_dun_tile(game_state, game_pass, p, tile_dims_screen_v, tile);
      }
      
      tile->flags &= ~(DunTileFlag_Illuminated | DunTileFlag_IlluminatedByPlayer);
    }
  }
  
  //r_O2D_rect_filled(game_pass, v2f_make(100, 500), v2f_make(50, 50), v4f_make(1, 0, 0, 1));
  r_O2D_rect_filled(game_pass, player_screen_p, v2f_scale(tile_dims_screen_v, 0.25f), rgba_make(1, 1, 1, 1));
  //R2D_Quad *quad = r2d_rect_filled(&r2d_buffer->game_quads, player_screen_p, v2f_scale(tile_dims_screen_v, 0.25f), rgba_make(1, 1, 1, 1), v2f_scale(tile_dims_screen_v, 0.25f).x * 0.5f);
  //quad->lighting_info |= LightInfo_EnableLightingForMe;
  //quad->lighting_info |= LightInfo_IsNotAffectedByLightIndex;
  //quad->lighting_info |= 0 << 2; // the light index its not affected by is 0
  
#if 1
  R_RenderPass *ui_pass = r_begin_pass(buffer, R_RenderPassType_UI);
  ui_begin(ui_state, dt_step, &(ui_pass->ui)); {
    loc b32 open_layout = false;
    
    ui_push_hori_layout(ui_state, str8("main buttons"), v2f_make_uniform(4.0f), v2f_make_uniform(4.0f)); {
      if (ui_do_button(ui_state, str8("Click Meh")).released) {
        open_layout = !open_layout;
      }
      
      ui_push_text_colour(ui_state, v4f_make(0.0f, 1.0f, 0.0f, 1.0f));
      if (ui_do_button(ui_state, str8("Regen Dungeon")).released) {
        s32 subdivs2 = 0;
        str8_to_s32(game_state->subdivs_depth_str, &subdivs2);
        dun_generate(main_arena, level->tiles, level->width, level->height, subdivs2);
      }
      ui_pop_text_colour(ui_state);
    } ui_pop_layout(ui_state);
    ui_push_hori_layout(ui_state, str8("dungeon gen parameters"), v2f_make(12, 4), v2f_make(0, 0)); {
      ui_push_border_colour(ui_state, rgba_make(0.0f, 0.0f, 0.0f, 1.0f));
      ui_push_background_colour(ui_state, rgba_make(0.0f, 0.0f, 0.0f, 1.0f));
      ui_do_label(ui_state, str8("Division Depth:"));
      ui_pop_background_colour(ui_state);
      ui_pop_border_colour(ui_state);
      
      ui_push_corner_roundness(ui_state, 2.0f);
      ui_do_int_input(ui_state, &game_state->subdivs_depth_str, &game_state->dun_gen_cursor);
      ui_pop_corner_roundness(ui_state);
      //ui_do_int_input(ui_state, &testing, &testing_cursor);
    } ui_pop_layout(ui_state);
    
    ui_push_corner_roundness(ui_state, 5.0f);
    if (open_layout) {
      ui_push_vert_layout(ui_state, str8("debug-stuff2"), v2f_make_uniform(16.0f), v2f_make_uniform(8.0f)); {
        
        ui_push_transparent_background(ui_state);
        ui_push_edge_thickness(ui_state, 0.0f);
        ui_push_offset_x(ui_state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Percentage, 0.25f));
        ui_do_label(ui_state, str8("Debug Stuff"));
        ui_pop_offset_x(ui_state);
        ui_pop_edge_thickness(ui_state);
        ui_pop_transparent_background(ui_state);
        
        ui_push_vert_layout(ui_state, str8("debug-stuff"), v2f_make(4.0f, 4.0f), v2f_make(0.0f, 8.0f)); {
          ui_push_corner_roundness(ui_state, 8.0f);
          ui_do_labelf(ui_state, str8("UI Hot Key: %llu"), ui_state->hot_key.key);
          ui_do_labelf(ui_state, str8("UI Active Key: %llu"), ui_state->active_key.key);
          ui_do_labelf(ui_state, str8("UI Focus Hot Key: %llu"), ui_state->focus_hot_key.key);
          ui_do_labelf(ui_state, str8("UI Focus Active Key: %llu"), ui_state->focus_active_key.key);
          ui_do_labelf(ui_state, str8("UI widget arena allocs: %llu"), ui_state->widget_arena->stack_ptr);
          ui_do_labelf(ui_state, str8("UI widget util allocs: %llu"), ui_state->util_arena->stack_ptr);
          ui_do_labelf(ui_state, str8("Player Current level: %llu"), game_state->player_current_level);
          if (ui_do_button(ui_state, str8("Toggle Lights: TODO IMPLEMENT")).released) {
            game_state->enable_lights = !game_state->enable_lights;
            //r2d_buffer->light_constants.enable_lights[0] = !r2d_buffer->light_constants.enable_lights[0];
          }
          
          if (ui_do_button(ui_state, str8("Toggle Tile Occlusion")).released) {
            game_state->debug_enabled_tile_occulution = !game_state->debug_enabled_tile_occulution;
          }
          
          ui_pop_corner_roundness(ui_state);
        } ui_pop_layout(ui_state);
      } ui_pop_layout(ui_state);
    }
    ui_pop_corner_roundness(ui_state);
    if (ui_do_button(ui_state, str8("AntiDupe###Add 10 exp")).released) {
      player->exp += 15;
      if (player->exp >= player->max_exp) {
        player->level += 1;
        player->stat_pts_to_spend += 3;
        while (player->exp >= player->max_exp) {
          u64 excess = player->exp - player->max_exp;
          player->exp = excess;
          player->max_exp = ent_evaluate_exp_function(player);
        }
      }
    }
    
    game_ui_player_info(game_state, input);
  } ui_end(ui_state);
#endif
  
  /*
  r2d_buffer->light_constants.ambient_colour = v4f_make(0.02f, 0.02f, 0.02f, 1.0f);
  //r2d_buffer.light_constants.enable_lights[0] = 1;
  r2d_buffer->light_constants.lights[0].p = v4f_make(player_screen_p.x, player_screen_p.y, 0.0f, 0.0f);
  r2d_buffer->light_constants.lights[0].colour = v4f_make(1, 0.8f, 0.0f, 1);
  r2d_buffer->light_constants.lights[0].constant_attenuation = 1.0f;
  r2d_buffer->light_constants.lights[0].linear_attenuation = 0.03f;
  r2d_buffer->light_constants.lights[0].type = LightType_Point;
  r2d_buffer->light_constants.lights[0].is_enabled = true;
  */
  prof_end_timed_block();
}

fun void
game_init_player(Game_State *game_state) {
  Entity *player = &(game_state->player);
  player->type = EntityType_Player;
  player->p = v2f_make(64, 64);
  player->stat_pts_to_spend = 0;
  
  // base stats.
  // TODO: soon this should be based on player class
  player->attributes[AttributeType_MaxHP].value_f32 = 40.0f;
  player->attributes[AttributeType_HealPerSecond].value_f32 = 0.20f;
  
  // we should think about what these value represent. I think this should be percentage increase 
  // in base damages of weapons / abilities
  player->attributes[AttributeType_PhysicalDamage].value_f32 = 5.0f;
  player->attributes[AttributeType_SpecialDamage].value_f32 = 5.0f;
  
  // we should think about what these value represent. I think this should be percentage increase 
  // in base defences of armours.
  player->attributes[AttributeType_PhysicalDefence].value_f32 = 5.0f;
  player->attributes[AttributeType_SpecialDefence].value_f32 = 5.0f;
  
  player->attributes[AttributeType_Accuracy].value_f32 = 3.0f;
  
  player->health = player->attributes[AttributeType_MaxHP].value_f32 * 0.5f;
  
  player->level = 1;
  player->exp = 0;
  player->max_exp = 10;
}

fun void
test_audio_callback(u32 buffer_sz_bytes, u8 *buffer_ptr, void *user_data) {
  Sound *sound = (Sound *)user_data;
  
  if ((sound->sound_index_in_bytes + buffer_sz_bytes) < sound->size_in_bytes) {
    s16 *out_chunk = (s16 *)buffer_ptr;
    u32 sample_count_for_buffer = buffer_sz_bytes / audio_bytes_per_channel();
    for (u32 sample_idx = 0; sample_idx < sample_count_for_buffer; ++sample_idx) {
      u64 current_sample = sample_idx + sound->sound_index_in_bytes / audio_bytes_per_channel();
      u8 *sample_out = (u8 *)(sound->data + current_sample);
      out_chunk[sample_idx] = sample_out[0] | (sample_out[1] << 8);
    }
    
    sound->sound_index_in_bytes += buffer_sz_bytes;
  } else {
    clear_memory(buffer_ptr, buffer_sz_bytes);
  }
}

typedef struct {
  v2f control_pts[4];
  R_CurveType type;
} Curve_Instance;

inl Curve_Instance
curve_create(R_CurveType type, v2f a, v2f b, v2f c, v2f d) {
  Curve_Instance result;
  result.control_pts[0] = a;
  result.control_pts[1] = b;
  result.control_pts[2] = c;
  result.control_pts[3] = d;
  result.type = type;
  return(result);
}

int __stdcall
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
  unused(hInstance);
  unused(hPrevInstance);
  unused(lpCmdLine);
  unused(nCmdShow);
  
  if (os_init()) {
    rnd_seed((u64)time(null));
    
    OS_Window *window = os_create_window(_str8(Video Game), 1280, 720);
    
    u64 frames_per_second = os_get_window_refresh_rate(window);
    f64 target_seconds_per_frame = 1.0 / (f64)frames_per_second;
    f32 game_update_seconds = (f32)target_seconds_per_frame;
    
#if 1
    
    
    R_Buffer *buffer = r_buffer_create(window);
    UI_State ui_state;
    ui_initialize(&ui_state, &window->input, null);
    b32 is_running = true;
    u64 ticks_begin_for_work = os_get_ticks();
    
    v2f mouse_delta = v2f_make(0, 0);
    Curve_Instance curve_instances[] = {
      curve_create(R_CurveType_Hermite, v2f_make(50, 50), v2f_make(500, 500),
                   v2f_make(25.0f, 100.0f), v2f_make(25.0f, -50.0f)),
      curve_create(R_CurveType_Bezier, v2f_make(300, 50), v2f_make(600, 50),
                   v2f_make(300, 100.0f), v2f_make(600, 100)),
    };
    
    u64 curve_current_drag = invalid_index_u64;
    u64 control_pt_current_drag = invalid_index_u64;
    
    r_viewport_set_dims(buffer, 1280, 720);
    while (is_running) {
      os_fill_events(window);
      if (os_get_misc_input_flag(&window->input, OSInputMisc_Quit) ||
          os_key_released(&window->input, OSInputKey_Escape)) {
        is_running = false;
      }
      
#if 0
      R_RenderPass *game_pass = r_begin_pass(buffer, R_RenderPassType_Game_Ortho2D);
      r_O2D_rect_filled(game_pass, v2f_make(300, 300), v2f_make(50, 50), v4f_make(1.0f, 0.0f, 0.0f, 1.0));
      r_O2D_rect_filled(game_pass, v2f_make(300, 275), v2f_make(50, 50), v4f_make(0.0f, 1.0f, 0.0f, 1.0));
      r_O2D_texture(game_pass, buffer->font[2].atlas, v2f_make(0, 0), v2f_make(512, 512), v4f_make(1.0f, 1.0f, 1.0f, 1.0));
      
      R_RenderPass *ui_pass = r_begin_pass(buffer, R_RenderPassType_UI);
      ui_state.pass = &(ui_pass->ui);
      ui_state.buffer = buffer;
      
      v4f colour = v4f_make(0.3f, 0.2f, 0.7f, 1.0f);
      v4f s_colour = v4f_make(0.5f, 0.1f, 0.1f, 1.0f);
      ui_rect(&ui_state, v2f_make(1000, 200), v2f_make(200, 200),
              ui_rect_colour(colour),
              100.0f, 0.0f, 1.0f,
              v2f_make(0, 0), v2f_make(0, 0), ui_rect_colour(s_colour),
              100.0f, 40.0f);
      
      ui_text(&ui_state, R_FontSize_Small, v2f_make(0, 400), v4f_make(0, 0, 1, 1), str8("hello world"));
#endif
      v2f mouse_p = os_get_mouse_v2f(&window->input);
      if (os_mouse_held(&window->input, OSInputMouse_Left) && (control_pt_current_drag == invalid_index_u64)) {
        for (u64 curve_instance_idx = 0;
             curve_instance_idx < array_count(curve_instances);
             ++curve_instance_idx) {
          b32 found_drag = false;
          Curve_Instance *instance = curve_instances + curve_instance_idx;
          v2f *control_pts = instance->control_pts;
          for (u64 control_pt_idx = 0;
               control_pt_idx < 4;
               ++control_pt_idx) {
            RectF32 rect;
            switch (instance->type) {
              case R_CurveType_Hermite: {
                if (control_pt_idx < 2) {
                  rect = rectf32_make(control_pts[control_pt_idx], v2f_make(10, 10));
                } else {
                  rect = rectf32_make(v2f_add(control_pts[control_pt_idx - 2], control_pts[control_pt_idx]), v2f_make(10, 10));
                }
              } break;
              
              case R_CurveType_Bezier: {
                rect = rectf32_make(control_pts[control_pt_idx], v2f_make(10, 10));
              } break;
            }
            
            if (point_in_rect(&mouse_p, &rect)) {
              curve_current_drag = curve_instance_idx;
              control_pt_current_drag = control_pt_idx;
              mouse_delta = v2f_sub(mouse_p, rect.p);
              found_drag = true;
              break;
            }
          }
          
          if (found_drag) {
            break;
          }
        }
      } else if (os_mouse_released(&window->input, OSInputMouse_Left)) {
        curve_current_drag = invalid_index_u64;
        control_pt_current_drag = invalid_index_u64;
      }
      
      if (curve_current_drag != invalid_index_u64) {
        switch (curve_instances[curve_current_drag].type) {
          case R_CurveType_Hermite: {
            if (control_pt_current_drag < 2) {
              curve_instances[curve_current_drag].control_pts[control_pt_current_drag] = v2f_sub(mouse_p, mouse_delta);
            } else if (control_pt_current_drag < 4) {
              curve_instances[curve_current_drag].control_pts[control_pt_current_drag] = v2f_sub(v2f_sub(mouse_p, curve_instances[curve_current_drag].control_pts[control_pt_current_drag - 2]), mouse_delta);
            }
          } break;
          
          case R_CurveType_Bezier: {
            curve_instances[curve_current_drag].control_pts[control_pt_current_drag] = v2f_sub(mouse_p, mouse_delta);
          } break;
        }
      }
      
      R_RenderPass *curve_pass = r_begin_pass_curve(buffer, array_count(curve_instances), 32);
      R_RenderPass *game_pass = r_begin_pass(buffer, R_RenderPassType_Game_Ortho2D);
      
      for (u64 curve_instance_idx = 0;
           curve_instance_idx < array_count(curve_instances);
           ++curve_instance_idx) {
        Curve_Instance *instance = curve_instances + curve_instance_idx;
        v2f *control_pts = instance->control_pts;
        
        switch (instance->type) {
          case R_CurveType_Hermite: {
            r_curve_add(curve_pass, instance->type, control_pts[0], control_pts[1],
                        v2f_scale(control_pts[2], 2.0f),
                        v2f_scale(control_pts[3], 2.0f));
            
            r_O2D_rect_filled(game_pass, control_pts[0], v2f_make(10, 10), v4f_make(1, 1, 1, 1));
            r_O2D_rect_filled(game_pass, control_pts[1], v2f_make(10, 10), v4f_make(1, 1, 1, 1));
            r_O2D_rect_filled(game_pass, v2f_add(control_pts[0], control_pts[2]), v2f_make(10, 10), v4f_make(0, 0, 1, 1));
            r_O2D_rect_filled(game_pass, v2f_add(control_pts[1], control_pts[3]), v2f_make(10, 10), v4f_make(0, 0, 1, 1));
          } break;
          
          case R_CurveType_Bezier: {
            r_curve_add(curve_pass, instance->type, control_pts[0], control_pts[1],
                        control_pts[2],
                        control_pts[3]);
            
            r_O2D_rect_filled(game_pass, control_pts[0], v2f_make(10, 10), v4f_make(0, 1, 0, 1));
            r_O2D_rect_filled(game_pass, control_pts[1], v2f_make(10, 10), v4f_make(0, 1, 0, 1));
            r_O2D_rect_filled(game_pass, control_pts[2], v2f_make(10, 10), v4f_make(0, 1, 0, 1));
            r_O2D_rect_filled(game_pass, control_pts[3], v2f_make(10, 10), v4f_make(0, 1, 0, 1));
          } break;
          
          default: {
            invalid_code_path();
          } break;
        }
        
      }
      
      r_submit_passes_to_gpu(buffer, true, 0, 0, 0, 1);
      
      u64 ticks_end_for_work = os_get_ticks();
      f64 seconds_elapsed_for_work = os_secs_between_ticks(ticks_begin_for_work, ticks_end_for_work);
      if (seconds_elapsed_for_work < target_seconds_per_frame) {
        os_sleep((u32)((target_seconds_per_frame - seconds_elapsed_for_work) * 1000.0f));
      }
      
      ticks_begin_for_work = os_get_ticks();
    }
#else
    R_Buffer *r_buffer = r_buffer_create(window);
    //r2d_initialize(&r2d_buffer, window);
    r_viewport_set_dims(r_buffer, 1280, 720);
    
    Game_State game_state = { 0 };
    game_state.main_arena = arena_reserve(mb(16), Memory_Arena_Flag_CommitOrDecommitOnPushOrPop);
    game_state.enable_lights = true;
    
    ui_initialize(&(game_state.ui_state), &window->input, r_buffer);
    game_state.sprite_sheet = r_texture_from_file(r_buffer, str8("..\\data\\assets\\texture\\tiles.png"));
    
    game_init_player(&game_state);
    game_state.camera = cam2d_make(v2f_make(1280.0f, 720.0f), v2f_make(2.0f, 2.0f), &game_state.player);
    
    game_state.has_initialized = true;
    
    game_state.subdivs = 4;
    game_state.subdivs_depth_str = str8_reserve_zero(game_state.main_arena, 1);
    
    game_state.stat_pts_t_dir = 1.0f;
    
    audio_init(44100);
    
    Sound sine_wave = { 0 };
    snd_generate_sine_wave(game_state.main_arena, &sine_wave, 30, 100, 5000);
    
    // What does this imply? We some sort of audio effect queue!
    audio_attatch_audio_provide_callback(&test_audio_callback, (void *)(&sine_wave));
    
    for (u32 level_index = 0;
         level_index < max_level_depth;
         level_index += 1) {
      Game_Level *level = game_state.levels + level_index;
      level->width = 52;
      level->height = 40;
      level->tiles = arena_push_array(game_state.main_arena, Dungeon_Tile, level->width * level->height);
      
      Dungeon_GenerateState state;
      Temporary_Memory temp = temp_mem_begin(game_state.main_arena);
      state.root = dun_generate_areas(game_state.main_arena, r32_make(v2i_make(0, 0), v2i_make(level->width, level->height)), game_state.subdivs);
      
      u32 total_tiles = level->width * level->height;
      
      for (u32 tile_index = 0; tile_index < total_tiles; tile_index += 1) {
        level->tiles[tile_index].flags = DunTileFlag_Walkable | DunTileFlag_IsNotDiscoveredByPlayer;
        level->tiles[tile_index].type = DunTile_Floor;
      }
      
      dun_generate_walls(state.root, level->tiles, level->width, 0);
      
      // TODO(christian): another think to think about: what if the up stairs is
      // close to down stairs?
      // NOTE(christian): if we are not last level, then don't add a down staircase!
      if (level_index < (max_level_depth - 1)) {
        // NOTE(christian): this is probably dumb for finding a free square for stair
        // plus, the stair could be infront of the door, thus the player couldn't 
        // get into room! But, we could go to corridor creation to avoid this!
        Memory_Arena *find_stair_memory = arena_get_scratch(&game_state.main_arena, 1);
        Temporary_Memory temp_stair_memory = temp_mem_begin(find_stair_memory);
        u32 index_candidate_count = 0;
        u32 *index_candidates = arena_push_array(find_stair_memory, u32, total_tiles);
        for (u32 tile_index = 0; tile_index < total_tiles; tile_index += 1) {
          Dungeon_Tile *tile = level->tiles + tile_index;
          if (tile->type == DunTile_Floor) {
            index_candidates[index_candidate_count++] = tile_index;
          }
        }
        
        assert_true(index_candidate_count > 0);
        u32 chosen_index = rnd_next_range_u32(0, index_candidate_count);
        Dungeon_Tile *tile = level->tiles + index_candidates[chosen_index];
        tile->type = DunTile_StairDown;
        level->stair_down_index = index_candidates[chosen_index];
        
        temp_mem_end(temp_stair_memory);
      }
      else {
        level->stair_down_index = -1;
      }
      
      // NOTE(christian): if we are not first level, don't add up staircase!
      if (level_index > 0) {
        Memory_Arena *find_stair_memory = arena_get_scratch(&game_state.main_arena, 1);
        Temporary_Memory temp_stair_memory = temp_mem_begin(find_stair_memory);
        u32 index_candidate_count = 0;
        u32 *index_candidates = arena_push_array(find_stair_memory, u32, total_tiles);
        
        for (u32 tile_index = 0; tile_index < total_tiles; tile_index += 1) {
          Dungeon_Tile *tile = level->tiles + tile_index;
          if (tile->type == DunTile_Floor) {
            index_candidates[index_candidate_count++] = tile_index;
          }
          
          tile->flags |= DunTileFlag_IsNotDiscoveredByPlayer;
        }
        
        assert_true(index_candidate_count > 0);
        u32 chosen_index = rnd_next_range_u32(0, index_candidate_count);
        Dungeon_Tile *tile = level->tiles + index_candidates[chosen_index];
        level->stair_up_index = index_candidates[chosen_index];
        tile->type = DunTile_StairUp;
        
        temp_mem_end(temp_stair_memory);
      } else {
        level->stair_up_index = -1;
      }
      
      dun_generate_vertical_corridors(state.root, level->tiles, level->width, 0);
      // NOTE(christian): I think this is fixed now
      dun_generate_horizontal_corridors(state.root, level->tiles, level->width, 0);
      
      temp_mem_end(temp);
    }
    
    //r2d_buffer.light_constants.enable_lights[0] = true;
    
    game_state.tile_dims = 32;
    game_state.debug_enabled_tile_occulution = true;
    
    b32 is_running = true;
    u64 ticks_begin_for_work = os_get_ticks();
    while (is_running) {
      os_fill_events(window);
      if (os_get_misc_input_flag(&window->input, OSInputMisc_Quit) ||
          os_key_released(&window->input, OSInputKey_Escape)) {
        is_running = false;
      }
      
      if (window->resized_this_frame) {
        cam2d_resize(&game_state.camera, v2f_make((f32)window->client_width, (f32)window->client_height));
      }
      
      game_update_and_render(&game_state, &window->input, r_buffer, game_update_seconds);
      r_submit_passes_to_gpu(r_buffer, true, 0.0f, 0.0f, 0.0f, 1.0f);
      
      
      u64 ticks_end_for_work = os_get_ticks();
      f64 seconds_elapsed_for_work = os_secs_between_ticks(ticks_begin_for_work, ticks_end_for_work);
      if (seconds_elapsed_for_work < target_seconds_per_frame) {
        os_sleep((u32)((target_seconds_per_frame - seconds_elapsed_for_work) * 1000.0f));
      }
      
      ticks_begin_for_work = os_get_ticks();
    }
#endif
    
  }
  os_exit_process(0);
}

Prof_TimerRecord prof_debug_records_array[__COUNTER__];