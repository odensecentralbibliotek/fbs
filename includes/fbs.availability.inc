<?php
/**
 * @file
 * Handles availability information from the library system.
 */

/**
 * Implements provider availability, items.
 *
 * @param array $provider_ids
 *   Array of ting object ids (faust).
 *
 * @return array
 *   Avaliability info.
 */
function fbs_availability_items($provider_ids)
{
  $avails = array();

  try {
    $avails = fbs_service()->Catalog->getAvailability(fbs_service()->agencyId, $provider_ids);
  } catch (Exception $e) {
    watchdog_exception('fbs', $e);
  }

  $result = array();
  foreach ($avails as $item) {
    $result[$item->recordId] = array('available' => $item->available, 'reservable' => $item->reservable);
  }

  return $result;
}

/**
 * Implements provider availability, holdings.
 *
 * @param array $provider_ids
 *   Array of ting object ids (faust).
 *
 * @return array
 *   Holdings info.
 */
function fbs_availability_holdings($provider_ids)
{
  $holdings = array();
  $excluded_branches_from_view = explode(PHP_EOL, variable_get('fbs_exclude_from_view', ''));

  /*
   * Convert to katalog posts.
   * sometimes the library staff will disagress with national placements.
   */

  try {
    $holdings = fbs_service()->Catalog->getHoldings(fbs_service()->agencyId, $provider_ids);
  } catch (Exception $e) {
    watchdog_exception('fbs', $e);
  }
  $result = array();
  $tmp_issues = array();
  foreach ($holdings as $holdings_index => $item) {
    $result[$item->recordId] = array(
      'reservable' => $item->reservable,
      // Gets properly set in the loop later.
      'available' => FALSE,
      'holdings' => array(),
      'total_count' => 0,
      // We don't know about this one.
      'is_internet' => FALSE,
      'reserved_count' => $item->reservations,
      'is_periodical' => FALSE,
    );

    $placement_parts = array(
      'branch',
      'department',
      'location',
      'sublocation',
      'materials',
    );
    foreach ($item->holdings as $branch_index => $item_holding) {

      $result_holding = array(
        'available_count' => 0,
        'total_count' => 0,
        // We have no idea about this either.
        'reference_count' => 0,
        // 'materials' added for use with IMS availability module
        'materials' => array(),
        'placement' => array(),
      );

      // Add in placement.
      foreach ($placement_parts as $part) {
        if (!empty($item_holding->{$part}->title)) {
          $result_holding['placement'][] = $item_holding->{$part}->title;
        }
      }

      $non_home_locations = explode(PHP_EOL, variable_get('fbs_nonreserveable_locations', ''));
      foreach ($result_holding['placement'] as $part) {
        foreach ($non_home_locations as $non_home) {
          if (!strcmp(trim($part), trim($non_home))) {
            for ($i = 0; $i < sizeof($item_holding->materials); $i++) {
              //$item_holding->materials[$i]->available = FALSE;
              $item_holding->materials[$i]->reserveable = FALSE;

            }
          }
        }
      }

      //check if it is in a non home location
      $non_home_locations = explode(PHP_EOL, variable_get('fbs_nonhome_locations', ''));
      foreach ($result_holding['placement'] as $part) {
        foreach ($non_home_locations as $non_home) {
          if (!strcmp(trim($part), trim($non_home))) {
            for ($i = 0; $i < sizeof($item_holding->materials); $i++) {
              $item_holding->materials[$i]->available = FALSE;

            }
            break;
          }
        }

      }
      // check if it is in a non home department
      $non_home_departments = explode(PHP_EOL, variable_get('fbs_nonhome_departments', ''));
      if (isset($item_holding->department) && in_array($item_holding->department->title, $non_home_departments)) {
        $item_holding->materials[0]->available = 0;
      }
      /*
       * we have to loop all materials , else it does not work for periodika.
       */
      $non_home_material_group = explode(PHP_EOL, variable_get('fbs_nonhome_material_groups', ''));
      foreach ($item_holding->materials as $index => &$material) {
        if (in_array($material->materialGroup->name, $non_home_material_group)) {
          $material->available = 0;
        }
        // add in the extra materials info available from later versions of the FBS api
        // which is necessary for using the IMS availability module 
        $result_holding['materials'][$index]['material_id'] = $material->itemNumber;
        $result_holding['materials'][$index]['periodical'] = $material->periodical;
        $result_holding['materials'][$index]['available'] = $material->available;
        $result_holding['materials'][$index]['materialGroupName'] = $material->materialGroup->name;
      }
      unset($material);
      // Add material description.
      $material_description = $item_holding->materials;
      if (variable_get('fbs_show_material_group', 0) && $material_description[0]) {
        $result_holding['placement'][] = $material_description[0]->materialGroupName;
      }
      if (!in_array($item->recordId, $provider_ids)) {
        continue;
      }

      $suffix_type = 'simple';
      if (isset($_POST['suffix_type_simplify']) && $_POST['suffix_type_simplify']) {

      } else {
        $suffix_type = variable_get('fbs_holdings_suffix_type', 'simple');
      }

      /*
       * Dont use the slower shelf_mark on the search pages...
       * we use HTTP_REFERER because it is an ajax call.
       */
      if ($suffix_type == 'simple' || (isset($_SESSION["inspiration_marcx_disable"]) && $_SESSION["inspiration_marcx_disable"])) {
        // Load entity by the provider id.

        $entity_id = $item->recordId; // we need to know the well id ?!
        $entity = ding_entity_load($entity_id);
        /* sometimes it fails to load the entity ? */
        if (isset($entity) && is_object($entity)) {
          // Get DK5 classification.
          if ($classification = $entity->getClassification()) {
            $result_holding['placement'][] = $classification;
          }

          // Get inverted author.
          if ($creator = $entity->getInvertedCreators()) {
            $result_holding['placement'][] = $creator[0];
          }
        }
      } elseif ($suffix_type == 'shelf_mark') {
        module_load_include('client.inc', 'ting');

        $entity_id = variable_get('ting_agency', FALSE) . "-katalog:" . $item->recordId;
        $entity = ting_get_object_marcxchange($entity_id);
        $suffix = '';
        $field652m = fbs_get_marc_field($entity, '652', 'm');
        $field652o = fbs_get_marc_field($entity, '652', 'o');
        $field652p = fbs_get_marc_field($entity, '652', 'p');
        $field654m = fbs_get_marc_field($entity, '654', 'm');

        if ($field652m != 'sk' && $field652o != 'sk') {
          // DK5 classification.
          if ($field652m != null) {
            $suffix .= $field652m;
          } else if ($field652o != null) {
            $suffix .= $field652o;
          } else {
            $suffix .= $field654m;
          }
          // Additonal descriptors from the DK5 classification.
          if ($field652b = fbs_get_marc_field($entity, '652', 'b')) {
            $suffix .= ' ' . $field652b;
          }
          if ($field652m == "99.4") {

            if ($field652a = fbs_get_marc_field($entity, '652', 'a')) {
              $suffix .= ' ' . $field652a;
              if ($field652h = fbs_get_marc_field($entity, '652', 'h')) {
                $suffix .= ', ' . $field652h;
              }
            }
          }

          // Additonal descriptors from the old DK5 classification.
          if ($field652m === null) {
            if ($field654b = fbs_get_marc_field($entity, '654', 'b')) {
              $suffix .= ' ' . $field654b;
            }
            if ($field654m == "99.4") {
              if ($field654a = fbs_get_marc_field($entity, '654', 'a')) {
                $suffix .= ' ' . $field654a;
                if ($field654h = fbs_get_marc_field($entity, '654', 'h')) {
                  $suffix .= ', ' . $field654h;
                }
              }
            }
          }

        }

        // Generate and append the standard suffix for ALMA shelf_mark. We prefer
        // marc fields in the following order:
        $candidates = array(
          '100.a',
          '110.a',
          '239.a',
          '245.a',
        );

        if ($shelf_mark = fbs_pick_marc_field($entity, $candidates)) {
          // If we found a shelf_mark and there's already something in suffix,
          // remember to add a seperator.
          if ($suffix) {
            if ($field652m != "99.4" && $field652p != "99.4") {
              $suffix .= ' > ';
            } else {
              $suffix .= " ";
            }
          }

          $words = explode(' ', $shelf_mark);
          $shelf_mark = FALSE;

          // Prefer words marked with a special sign.
          foreach ($words as &$word) {
            $word = trim($word);
            if (strpos($word, '¤') === 0) {
              $shelf_mark = mb_substr($word, 1);
            }
          }

          // If no special mark; use the first word.
          if (!$shelf_mark) {
            $shelf_mark = implode(' ', $words);
          }

          // A neat little trick for mb_ucfirst:
          // http://stackoverflow.com/questions/2517947/ucfirst-function-for-multibyte-character-encodings#answer-14161325.
          $shelf_mark = mb_strtoupper(mb_substr($shelf_mark, 0, 1)) . mb_strtolower(mb_substr($shelf_mark, 1));
          $suffix .= $shelf_mark;
        }

        $result_holding['placement'][] = $suffix;
      }

      $placement_string = implode(' > ', $result_holding['placement']);

      // Count available materials.
      settype($reserable_materials_count, "integer");
      foreach ($item_holding->materials as $mat) {
        $result[$item->recordId]['total_count']++;
        $result_holding['total_count']++;
        if ($mat->available) {
          $result_holding['available_count']++;
          $result[$item->recordId]['available'] = TRUE;
        }
        if (!isset($mat->reserveable) || $mat->reserveable == true) {
          $reserable_materials_count++;
        }

        if ($mat->periodical) {
          // Add to issues.
          $key = (!empty($mat->periodical->volumeYear) ?
            $mat->periodical->volumeYear : '') . ':' .
            $mat->periodical->volume . ':' .
            (!empty($mat->periodical->volumeNumber) ?
              $mat->periodical->volumeNumber : '') . ':' .
            $placement_string;
          if (isset($tmp_issues[$key])) {
            $tmp_issues[$key]['available'] += $mat->available ? 1 : 0;
            $tmp_issues[$key]['total'] += 1;
          } else {
            $tmp_issues[$key] = array(
              'vol' => $mat->periodical->volume . (!empty($mat->periodical->volumeNumber) ? '-' . $mat->periodical->volumeNumber : ''),
              'year' => $mat->periodical->volumeYear,
              'available' => $mat->available ? 1 : 0,
              'total' => 1,
              'placement' => $placement_string,
              'local_id' => _fbs_periodical_get_local_id($item->recordId, $mat->periodical),
            );
          }
        }
      }
      if ($reserable_materials_count == 0) {
        $result[$item->recordId]['reservable'] = false;
      } else {
        $result[$item->recordId]['reservable'] = true;
      }
      $result[$item->recordId]['holdings'][] = $result_holding;

      // Exclude branch from availability
      foreach ($excluded_branches_from_view as $branch) {
        if ($item_holding->branch->branchId == $branch) {
          unset($result[$item->recordId]['holdings'][$branch_index]);
        }
      }


    }

    // Add issues in the right nesting.
    if (!empty($tmp_issues)) {
      $issues = array();
      $result[$item->recordId]['is_periodical'] = TRUE;
      foreach ($tmp_issues as $issue) {
        // ding_periodical does not like null values.
        $year = isset($issue['year']) ? $issue['year'] : '0';
        if (!isset($issues[$year])) {
          $issues[$year] = array();
        }

        $year_collection = &$issues[$year];

        $vol = !empty($issue['vol']) ? $issue['vol'] : '0';
        if (!isset($year_collection[$vol])) {
          $year_collection[$vol] = array(
            'local_id' => $issue['local_id'],
            'placement' => array(),
          );
        }
        $year_collection[$vol]['placement'][] = array(
          'reservable' => $item->reservable,
          'available_count' => $issue['available'],
          'location' => $issue['placement'],
          'total_count' => $issue['total'],
        );
      }
      $result[$item->recordId]['issues'] = $issues;
    }

  }

  return $result;
}

/**
 *  Helper to get the value of the specified marc field.
 */
function fbs_get_marc_field($entity, $field, $subfield = NULL)
{
  if (!is_object($entity)) {
    return null;
  }
  $value = $entity->getValue($field, $subfield);

  // Ensure single value returned if subfield is specified.
  if (isset($subfield) && is_array($value)) {
    $value = reset($value);
  }

  return $value ? $value : NULL;
}

/**
 * Helper function to pick marc subfield from a range of candidates.
 */
function fbs_pick_marc_field($entity, array $candidates)
{
  foreach ($candidates as $candidate) {
    list($field, $subfield) = explode('.', $candidate);

    if ($field_value = fbs_get_marc_field($entity, $field, $subfield)) {
      return $field_value;
    }
  }
  return NULL;
}

/**
 * Tranlate a marc string code to a human readable label.
 */
function fbs_translate_marc($code, $field)
{
  $translations = _fbs_get_marc_039a_translations();

  if ($field == '039.a') {
    $translations = _fbs_get_marc_039a_translations();
  } else if ($field == '039.b') {
    $translations = _fbs_get_marc_039b_translations();
  }

  return isset($translations[$code]) ? $translations[$code] : NULL;
}

/**
 * Helper function to hold marc field 039.a machine code translations.
 *
 * See: http://www.kat-format.dk/danMARC2/Danmarc2.22.htm#pgfId=1574621
 */
function _fbs_get_marc_039a_translations()
{
  return array(
    'sam' => 'SAMLINGER',
    'mir' => 'MIDDELALDER/RENÆSSANCE',
    'mia' => 'MIDDELALDER/RENÆSSANCE. ANTOLOGIER',
    'avg' => 'AVANTGARDE',
    'ava' => 'AVANTGARDE. ANTOLOGIER',
    'ork' => 'ORKESTERMUSIK',
    'ora' => 'ORKESTERMUSIK. ANTOLOGIER',
    'kam' => 'KAMMERMUSIK',
    'kaa' => 'KAMMERMUSIK. ANTOLOGIER',
    'eti' => 'ET INSTRUMENT',
    'eta' => 'ET INSTRUMENT. ANTOLOGIER',
    'vok' => 'VOKALMUSIK',
    'voa' => 'VOKALMUSIK. ANTOLOGIERø',
    'opr' => 'OPERAER',
    'opa' => 'OPERAER. ANTOLOGIER',
    'otm' => 'OPERETTER/MUSICALS',
    'ota' => 'OPERETTER/MUSICALS. ANTOLOGIER',
    'new' => 'NEW AGE',
    'fol' => 'FOLKEMUSIK',
    'blu' => 'BLUES',
    'bla' => 'BLUES. ANTOLOGIER',
    'jaz' => 'JAZZ',
    'jaa' => 'JAZZ. ANTOLOGIER',
    'bef' => 'ROCK',
    'bea' => 'ROCK. ANTOLOGIER',
    'und' => 'UNDERHOLDNING',
    'dau' => 'DANSK UNDERHOLDNING',
    'fau' => '**FÆRØSK UNDERHOLDNING',
    'jul' => 'JULEMUSIK',
    'fil' => 'FILMMUSIK',
    'tes' => 'TESTPLADER',
    'mmo' => 'MUSIC MINUS ONE',
    'hib' => 'HISTORIER. BØRN',
    'mub' => 'MUSIK. BØRN',
  );
}

/**
 * Helper function to hold marc field 039.b machine code translations.
 *
 * See: http://www.kat-format.dk/danMARC2/Danmarc2.22.htm#pgfId=1574621
 */
function _fbs_get_marc_039b_translations()
{
  return array(
    'ad' => 'Andorra',
    'ae' => 'Forenede Arabiske Emirater',
    'af' => 'Afghanistan',
    'ag' => 'Antigua og Barbuda',
    'ai' => 'Anguilla',
    'al' => 'Albanien',
    'am' => 'Armenien',
    'ao' => 'Angola',
    'aq' => 'Antarktis',
    'ar' => 'Argentina',
    'as' => 'Amerikansk Samoa',
    'at' => 'Østrig',
    'au' => 'Australien',
    'aw' => 'Aruba',
    'az' => 'Aserbajdsjan',
    'ba' => 'Bosnien-Hercegovina',
    'bb' => 'Barbados',
    'bd' => 'Bangladesh',
    'be' => 'Belgien',
    'bf' => 'Burkina Faso',
    'bg' => 'Bulgarien',
    'bh' => 'Bahrein',
    'bi' => 'Burundi',
    'bj' => 'Benin',
    'bm' => 'Bermuda',
    'bn' => 'Brunei',
    'bo' => 'Bolivia',
    'bq' => 'Bonaire, Sint Eustatius og  Saba',
    'br' => 'Brasilien',
    'bs' => 'Bahamas',
    'bt' => 'Bhutan',
    'bv' => 'Bouvetøen',
    'bw' => 'Botswana',
    'by' => 'Hviderusland (Belarus)',
    'bz' => 'Belize',
    'ca' => 'Canada',
    'cc' => 'Kokosøerne',
    'cf' => 'Centralafrikanske Republik',
    'cg' => 'Congo',
    'ch' => 'Svejts',
    'ci' => 'Elfenbenskysten',
    'ck' => 'Cookøerne',
    'cl' => 'Chile',
    'cm' => 'Cameroun',
    'cn' => 'Kina',
    'co' => 'Colombia',
    'cr' => 'Costa Rica',
    'cu' => 'Cuba',
    'cv' => 'Kap Verde',
    'cw' => 'Curaçao',
    'cx' => 'Christmas Island',
    'gb' => 'Storbritannien',
    'gd' => 'Grenada',
    'ge' => 'Georgien',
    'gf' => 'Fransk Guyana',
    'gh' => 'Ghana',
    'gi' => 'Gibraltar',
    'gl' => 'Grønland',
    'gm' => 'Gambia',
    'gn' => 'Guinea',
    'gp' => 'Guadeloupe',
    'gq' => 'Ækvatorialguinea',
    'gr' => 'Grækenland',
    'gs' => 'South Georgia og De Sydlige ',
    'Sa' => 'dwichøer',
    'gt' => 'Guatemala',
    'gu' => 'Guam',
    'gw' => 'Guinea-Bissau',
    'gy' => 'Guyana',
    'hk' => 'Hongkong',
    'hm' => 'Heard- og McDonaldøerne',
    'hn' => 'Honduras',
    'hr' => 'Kroatien',
    'ht' => 'Haiti',
    'hu' => 'Ungarn',
    'id' => 'Indonesien',
    'ie' => 'Irland',
    'il' => 'Israel',
    'in' => 'Indien',
    'io' => 'Britiske Territorium i Det Indiske Ocean',
    'iq' => 'Irak',
    'ir' => 'Iran',
    'is' => 'Island',
    'it' => 'Italien',
    'jm' => 'Jamaica',
    'jo' => 'Jordan',
    'jp' => 'Japan',
    'ke' => 'Kenya',
    'kg' => 'Kirgisistan',
    'kh' => 'Cambodja',
    'ki' => 'Kiribati',
    'km' => 'Comorerne',
    'kn' => 'St. Christopher og Nevis',
    'kp' => 'Nordkorea',
    'kr' => 'Sydkorea',
    'kw' => 'Kuwait',
    'ky' => 'Caymanøerne',
    'kz' => 'Kasakhstan',
    'la' => 'Laos',
    'lb' => 'Libanon',
    'lc' => 'St. Lucia',
    'li' => 'Liechtenstein',
    'lk' => 'Sri Lanka',
    'lr' => 'Liberia',
    'ls' => 'Lesotho',
    'lt' => 'Litauen',
    'lu' => 'Luxembourg',
    'lv' => 'Letland',
    'ly' => 'Libyen',
    'ma' => 'Marokko',
    'mc' => 'Monaco',
    'md' => 'Moldova',
    'mg' => 'Madagaskar',
    'mh' => 'Marshalløerne',
    'mk' => 'Makedonien, Den tidligere jugoslaviske republik',
    'ml' => 'Mali',
    'mm' => 'Myanmar',
    'mn' => 'Mongoliet',
    'mo' => 'Macao',
    'mp' => 'Nordmarianerne',
    'mq' => 'Martinique',
    'mr' => 'Mauretanien',
    'ms' => 'Montserrat',
    'mt' => 'Malta',
    'mu' => 'Mauritius',
    'mv' => 'Maldiverne',
    'mw' => 'Malawi',
    'mx' => 'Mexico',
    'my' => 'Malaysia',
    'mz' => 'Mozambique',
    'na' => 'Namibia',
    'nc' => 'Ny Caledonien',
    'ne' => 'Niger',
    'nf' => 'Norfolk Island',
    'ng' => 'Nigeria',
    'ni' => 'Nicaragua',
    'nl' => 'Holland',
    'no' => 'Norge',
    'np' => 'Nepal',
    'nr' => 'Nauru',
    'nu' => 'Niue',
    'nz' => 'New Zealand',
    'om' => 'Oman',
    'pa' => 'Panama',
    'pe' => 'Peru',
    'pf' => 'Fransk Polynesien',
    'pg' => 'Papua Ny Guinea',
    'ph' => 'Filippinerne',
    'pk' => 'Pakistan',
    'pl' => 'Polen',
    'pm' => 'Saint Pierre et Miquelon',
    'pn' => 'Pitcairn',
    'pr' => 'Puerto Rico',
    'ps' => 'Palæstina',
    'pt' => 'Portugal',
    'pw' => 'Palau',
    'py' => 'Paraguay',
    'qa' => 'Qatar',
    're' => 'Réunion',
    'ro' => 'Rumænien',
    'ru' => 'Rusland',
    'rw' => 'Rwanda',
    'sa' => 'Saudi-Arabien',
    'sb' => 'Salomonøerne',
    'sc' => 'Seychellerne',
    'sd' => 'Sudan',
    'se' => 'Sverige',
    'sg' => 'Singapore',
    'sh' => 'Sankt Helena',
    'si' => 'Slovenien',
    'sj' => 'Svaldbard og Jan Mayen',
    'sk' => 'Slovakiet',
    'sl' => 'Sierra Leone',
    'sm' => 'San Marino',
    'sn' => 'Senegal',
    'so' => 'Somalia',
    'sr' => 'Surinam',
    'ss' => 'Sydsudan',
    'st' => 'Sao Tomé og Principe',
    'sv' => 'El Salvador',
    'sx' => 'Sint Maarten (Hollandsk del)',
    'sy' => 'Syrien',
    'sz' => 'Swaziland',
    'tc' => 'Turks- og Caicosøerne',
    'td' => 'Tchad',
    'tf' => 'Franske Territorier, Sydlige',
    'tg' => 'Togo',
    'th' => 'Thailand',
    'tj' => 'Tadsjikistan',
    'tk' => 'Tokelau',
    'tm' => 'Turkmenistan',
    'tn' => 'Tunesien',
    'to' => 'Tonga',
    'tp' => 'Østtimor',
    'tr' => 'Tyrkiet',
    'tt' => 'Trinidad og Tobago',
    'tv' => 'Tuvalu',
    'tw' => 'Taiwan',
    'tz' => 'Tanzania',
    'ua' => 'Ukraine',
    'ug' => 'Uganda',
    'um' => 'Amerikanske Oversøiske Øer, Mindre',
    'us' => 'USA',
    'uy' => 'Uruguay',
    'uz' => 'Usbekistan',
    'va' => 'Vatikanstaten',
    'vc' => 'St. Vincent og Grenadinerne',
    've' => 'Venezuela',
    'vg' => 'Jomfruøerne, Britiske',
    'vi' => 'Jomfruøerne, Amerikanske',
    'vn' => 'Vietnam',
    'vu' => 'Vanuatu',
    'wf' => 'Wallis-og Futunaøerne',
    'ws' => 'Samoa',
    'ye' => 'Yemen',
    'yt' => 'Mayotte',
    'yu' => 'Jugoslavien samt Serbien og Montenegro',
    'za' => 'Sydafrika',
    'zm' => 'Zambia',
    'zr' => 'Zaire',
    'zw' => 'Zimbabwe',
  );
}
