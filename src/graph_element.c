#include "app_keys.h"
#include "config.h"
#include "graph_element.h"
#include "layout.h"
#include "preferences.h"
#include "staleness.h"

static void plot_point(int x, int y, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(x, y, GRAPH_POINT_SIZE, GRAPH_POINT_SIZE), 0, GCornerNone);
}

static int bg_to_y(int height, int bg, int min, int max, bool fit_in_bounds) {
  // Graph lower bound, graph upper bound
  int graph_min = get_prefs()->bottom_of_graph;
  int graph_max = get_prefs()->top_of_graph;
  int y = (float)height - (float)(bg - graph_min) / (float)(graph_max - graph_min) * (float)height - 1.0f;
  if (fit_in_bounds) {
    if (y < min) {
      y = min;
    } else if (y > max) {
      y = max;
    }
  }
  return y;
}

static int bg_to_y_for_point(int height, int bg) {
  return bg_to_y(height, bg, 0, height - GRAPH_POINT_SIZE, true);
}

static int bg_to_y_for_line(int height, int bg) {
  return bg_to_y(height, bg, -1, height - 1, false);
}

static void graph_update_proc(Layer *layer, GContext *ctx) {
  int i, x, y;
  GSize size = layer_get_bounds(layer).size;

  GraphData *data = layer_get_data(layer);
  int padding = graph_staleness_padding();
  for(i = 0; i < data->count; i++) {
    // XXX: JS divides by 2 to fit into 1 byte
    int bg = data->sgvs[i] * 2;
    if(bg == 0) {
      continue;
    }
    x = size.w - GRAPH_POINT_SIZE * (1 + i + padding);
    y = bg_to_y_for_point(size.h, bg);
    plot_point(x, y, ctx);
  }

  // Target range bounds
  uint16_t limits[2] = {get_prefs()->top_of_range, get_prefs()->bottom_of_range};
  for(i = 0; i < (int)ARRAY_LENGTH(limits); i++) {
    y = bg_to_y_for_line(size.h, limits[i]);
    for(x = 0; x < size.w; x += 4) {
      graphics_draw_line(ctx, GPoint(x, y), GPoint(x + 2, y));
    }
  }

  // Horizontal gridlines
  int h_gridline_frequency = get_prefs()->h_gridlines;
  if (h_gridline_frequency > 0) {
    int graph_min = get_prefs()->bottom_of_graph;
    int graph_max = get_prefs()->top_of_graph;
    for(int g = 0; g < graph_max; g += h_gridline_frequency) {
      if (g <= graph_min || g == limits[0] || g == limits[1]) {
        continue;
      }
      y = bg_to_y_for_line(size.h, g);
      for(x = 2; x < size.w; x += 8) {
        graphics_draw_line(ctx, GPoint(x, y), GPoint(x + 1, y));
      }
    }
  }
}

GraphElement* graph_element_create(Layer *parent) {
  GRect bounds = element_get_bounds(parent);

  Layer* graph_layer = layer_create_with_data(
    GRect(0, 0, bounds.size.w, bounds.size.h),
    sizeof(GraphData)
  );
  ((GraphData*)layer_get_data(graph_layer))->sgvs = malloc(GRAPH_MAX_SGV_COUNT * sizeof(char));
  layer_set_update_proc(graph_layer, graph_update_proc);
  layer_add_child(parent, graph_layer);

  ConnectionStatusComponent *conn_status = connection_status_component_create(parent, 1, 1);

  GraphElement *el = malloc(sizeof(GraphElement));
  el->graph_layer = graph_layer;
  el->conn_status = conn_status;
  return el;
}

void graph_element_destroy(GraphElement *el) {
  free(((GraphData*)layer_get_data(el->graph_layer))->sgvs);
  layer_destroy(el->graph_layer);
  connection_status_component_destroy(el->conn_status);
  free(el);
}

void graph_element_update(GraphElement *el, DictionaryIterator *data) {
  int count = dict_find(data, APP_KEY_SGV_COUNT)->value->int32;
  count = count > GRAPH_MAX_SGV_COUNT ? GRAPH_MAX_SGV_COUNT : count;
  ((GraphData*)layer_get_data(el->graph_layer))->count = count;
  memcpy(
    ((GraphData*)layer_get_data(el->graph_layer))->sgvs,
    (char*)dict_find(data, APP_KEY_SGVS)->value->cstring,
    count * sizeof(char)
  );
  layer_mark_dirty(el->graph_layer);
  connection_status_component_refresh(el->conn_status);
}

void graph_element_tick(GraphElement *el) {
  connection_status_component_refresh(el->conn_status);
}
