#include "../xtreme_settings_app.h"
#include <lib/toolbox/value_index.h>

#define CYCLE_ANIMATION_COUNT 13
const char* const cycle_animation_names[CYCLE_ANIMATION_COUNT] = {
    "OFF",
    "Manifest",
    "30 S",
    "1 M",
    "5 M",
    "10 M",
    "15 M",
    "30 M",
    "1 H",
    "2 H",
    "6 H",
    "12 H",
    "24 H",
};

const int32_t cycle_animation_values[CYCLE_ANIMATION_COUNT] =
    {-1, 0, 30, 60, 300, 600, 900, 1800, 3600, 7200, 21600, 43200, 86400};

static void xtreme_settings_scene_start_cycle_animation_changed(VariableItem* item) {
    XtremeSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, cycle_animation_names[index]);
    app->settings.cycle_animation = cycle_animation_values[index];
}

void xtreme_settings_scene_start_on_enter(void* context) {
    XtremeSettingsApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;
    uint8_t value_index;

    item = variable_item_list_add(
        var_item_list,
        "Cycle Animation",
        CYCLE_ANIMATION_COUNT,
        xtreme_settings_scene_start_cycle_animation_changed,
        app);

    value_index = value_index_int32(
        app->settings.cycle_animation, cycle_animation_values, CYCLE_ANIMATION_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cycle_animation_names[value_index]);

    view_dispatcher_switch_to_view(app->view_dispatcher, XtremeSettingsAppViewVarItemList);
}

bool xtreme_settings_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

void xtreme_settings_scene_start_on_exit(void* context) {
    XtremeSettingsApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
