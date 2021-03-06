<?php
/**
 * @file
 * fbs.features.inc
 */

/**
 * Implements hook_default_profile2_type().
 */
function fbs_default_profile2_type() {
  $items = array();
  $items['provider_fbs'] = entity_import('profile2_type', '{
    "userCategory" : true,
    "userView" : true,
    "type" : "provider_fbs",
    "label" : "Profil",
    "weight" : "0",
    "data" : { "registration" : 0, "use_one_page" : 1, "use_page" : 0 }
  }');
  return $items;
}
