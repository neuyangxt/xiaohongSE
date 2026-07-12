#ifndef XH_SCENE_RULE_H
#define XH_SCENE_RULE_H

#include <stdbool.h>
#include <stdint.h>

void xh_scene_rule_init(void);
void xh_scene_rule_tick(void);
bool xh_scene_rule_set_mode(uint8_t mode, const char *source);
uint8_t xh_scene_rule_get_mode(void);
uint8_t xh_scene_rule_get_cause(void);
uint16_t xh_scene_rule_pack_report(uint8_t *out, uint16_t cap);
void xh_scene_rule_on_sensor_update(uint8_t module_id);
void xh_scene_rule_on_manual_fan_control(uint8_t level, const char *source);
void xh_scene_rule_on_fan_report_transition(uint8_t old_level, uint8_t new_level,
                                            const char *source);
void xh_scene_rule_on_light_report(uint8_t mode, const char *source);
void xh_scene_rule_on_module_ready(uint8_t module_id);

#endif
