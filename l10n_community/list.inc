<?php

function l10n_community_list_page($langcode) {
  require_once 'translate.inc';

  drupal_add_css(drupal_get_path('module', 'l10n_community') .'/list/l10n.css');
  drupal_add_js(drupal_get_path('module', 'l10n_community') .'/list/jquery13.js');
  drupal_add_js(drupal_get_path('module', 'l10n_community') .'/list/jquery.worddiff.js');
  drupal_add_js(drupal_get_path('module', 'l10n_community') .'/list/l10n.js');

  $language = l10n_community_get_language($langcode);
  $filters = l10n_community_build_filter_values($_GET);
  $strings = l10n_community_get_strings($language->language, $filters, $filters['limit']);

  $output = drupal_get_form('l10n_community_filter_form', $filters);

  if (!count($strings)) {
    drupal_set_message(t('No strings found with this filter. Try adjusting the filter options.'));
  }
  else {
    $output .= drupal_get_form('l10n_community_list_form', $language, $filters, $strings);
  }

  return $output;
}

function l10n_community_list_form(&$form_state, $language, $filters, $strings) {
  $permission = l10n_community_get_permission($language->language);
  $pager = theme('pager', NULL, $filters['limit'], 0);
  $redirect_url = $_GET;
  unset($redirect_url['q']);

  $form = array(
    '#submit' => array('l10n_community_list_submit'),
    '#redirect' => array($_GET['q'], $redirect_url),
    'langcode' => array('#type' => 'value', '#value' => $language->language),
    'pager_top' => array('#weight' => -10, '#value' => $pager),
    'submit_top' => array('#type' => 'submit', '#value' => t('Save changes'), '#access' => $permission !== L10N_PERMISSION_VIEW),
    'strings' => array('#tree' => TRUE, '#theme' => 'l10n_community_list_table'),
    'submit' => array('#type' => 'submit', '#value' => t('Save changes'), '#access' => $permission !== L10N_PERMISSION_VIEW),
    'pager_bottom' => array('#weight' => 10, '#value' => $pager),
  );

  foreach ($strings as $string) {
    $form['strings'][$string->sid] = _l10n_community_list_string($form_state, $string, $language, $permission);
  }

  return $form;
}

function _l10n_community_list_render_string($strings, $empty = '') {
  if ($empty) {
    $empty = ' data-empty="'. check_plain($empty) .'"';
  }
  return "<span$empty>". implode("</span><br /><span$empty>", array_map('check_plain', $strings)) .'</span>';
}

function _l10n_community_list_string(&$form_state, $source, $language, $permission) {
  // Normalize empty default translation.
  if (!$source->translation) {
    $source->tid = '0';
    $source->translation = array(t('(not translated)'));
    $source->is_active = '1';
    $source->is_suggestion = '0';
  }
  else {
    $source->translation = _l10n_community_list_unpack_string($source->translation);
  }

  $source->value = _l10n_community_list_unpack_string($source->value);

  $form = array(
    '#string' => $source,
    '#langcode' => $language->language,
    'source' => array('#value' => _l10n_community_list_render_string($source->value))
  );

  // Add the current string (either a approved translation or a mock object
  // for the "untranslated" string).
  $form[$source->tid] = _l10n_community_list_translation($form_state, $source, $permission, $source);

  // When there are suggestions, load them from the database.
  if ($source->has_suggestion) {
    $suggestions = _l10n_community_get_suggestions($source, $language);
    foreach ($suggestions as $tid => $suggestion) {
      $form[$tid] = _l10n_community_list_translation($form_state, $suggestion, $permission, $source);
    }
  }

  if ($permission & L10N_PERMISSION_SUGGEST) {
    $textarea = _l10n_community_list_translation_textarea($source, $language);
    $form[$textarea->tid] = _l10n_community_list_translation($form_state, $textarea, $permission, $source);
  }

  return $form;
}

// Build mock object for new textarea
function _l10n_community_list_translation_textarea($source, $language) {
  global $user;

  return (object)array(
    'sid' => $source->sid,
    'tid' => 'new',
    'translation' => array_fill(0, count($source->value), ''),
    'is_active' => '1',
    'is_suggestion' => '1',
    'uid_entered' => $user->uid,
  );
}


function _l10n_community_list_translation(&$form_state, $string, $permission, $source) {
  global $user;
  $is_own = $user->uid == $string->uid_entered;
  $is_active = $string->is_active && !$string->is_suggestion;
  $is_new = $string->tid == 'new';
  $may_moderate = $permission & ($is_own ? L10N_PERMISSION_MODERATE_OWN : L10N_PERMISSION_MODERATE_OTHERS);
  $may_stabilize = $permission & ($is_own ? L10N_PERMISSION_STABILIZE_OWN : L10N_PERMISSION_STABILIZE_OTHERS);

  $form = array(
    '#theme' => 'l10n_community_list_translation',
    'original' => array('#type' => 'value', '#value' => $string),
  );

  $form['active'] = array(
    '#type' => 'radio',
    '#theme' => 'l10n_community_list_radio',
    '#title' => _l10n_community_list_render_string($string->translation, $is_new ? t('(empty)') : FALSE),
    '#return_value' => $string->tid,
    '#default_value' => $is_active ? $string->tid : NULL,
    '#parents' => array('strings', $string->sid, 'active'),
    '#disabled' => !$may_moderate && !$is_active,
    '#attributes' => array('class' => 'selector'),
  );

  if ($string->tid) {
    if ($may_moderate && $string->tid != 'new') {
      $form['declined'] = array(
        '#type' => 'checkbox',
        '#title' => t('Declined'),
        '#default_value' => !($string->is_active || $string->is_suggestion),
      );
    }
    // if ($may_stabilize) {
    //   $form['stable'] = array(
    //     '#type' => 'checkbox',
    //     '#title' => t('Stable'),
    //     '#default_value' => FALSE, // $string->is_stable,
    //   );
    // }
    if ($string->tid == 'new') {
      $form['value'] = array_fill(0, count($source->value), array(
        '#type' => 'textarea',
        '#cols' => 60,
        '#rows' => 3,
        '#default_value' => t('<New translation>'),
      ));
    }
    else {
      if ($permission & L10N_PERMISSION_SUGGEST) {
        $form['edit'] = array(
          '#value' => t('Edit Copy'),
          '#prefix' => '<label title="'. t('Edit Copy') .'">',
          '#suffix' => '</label>',
        );
      }
      if (isset($string->username)) {
        $now = time();
        $params = array(
          '!author' => theme('username', (object)array('name' => $string->username, 'uid' => $string->uid_entered)),
          '@date' => format_date($string->time_entered),
          '@ago' => t('@time ago', array('@time' => format_interval($now - $string->time_entered))),
        );

        if ($string->is_suggestion) {
          $title = t('suggested by !author <span title="@ago">on @date</span>', $params);
        }
        else {
          $title = t('translated by !author <span title="@ago">on @date</span>', $params);
        }
        if (!empty($string->username_approved)) {
          $title .= '<br />'. t('approved by !author <span title="@ago">on @date</span>', array(
            '!author' => theme('username', (object)array('name' => $string->username_approved, 'uid' => $string->uid_approved)),
            '@date' => format_date($string->time_approved),
            '@ago' => t('@time ago', array('@time' => format_interval($now - $string->time_approved))),
          ));
        }

        $form['author'] = array(
          '#value' => $title,
        );
      }
    }
  }

  return $form;
}


function theme_l10n_community_list_translation($element) {
  if (!isset($element['#attributes']['class'])) {
    $element['#attributes']['class'] = '';
  }
  $element['#attributes']['class'] .= ' clearfix translation';

  switch ($element['active']['#return_value']) {
    case 'new':
      $element['#attributes']['class'] .= ' new-translation';
      break;
    case '0':
      $element['#attributes']['class'] .= ' no-translation';
      // Fallthrough.
    default:
      if ($element['active']['#value'] !== '') {
        $element['#attributes']['class'] .= ' is-active default';
      }
  }

  $output = '<li'. drupal_attributes($element['#attributes']) .'>';

  $actions = '';
  foreach (array('declined', /*'stable', */'edit') as $type) {
    if (isset($element[$type])) {
      $actions .= '<li class="'. $type .'">'. drupal_render($element[$type]) .'</li>';
    }
  }
  if (!empty($actions)) {
    $output .= '<ul class="actions">'. $actions .'</ul>';
  }

  $output .= drupal_render($element['active']);

  if (isset($element['author'])) {
    $output .= '<div class="author">'. drupal_render($element['author']) .'</div>';
  }

  if (isset($element['value'])) {
    $output .= drupal_render($element['value']);
  }

  return $output  . '</li>';
}

function theme_l10n_community_list_radio($element) {
  _form_set_class($element, array('form-radio'));
  $output = '<input type="radio" ';
  $output .= 'id="'. $element['#id'] .'" ';
  $output .= 'name="'. $element['#name'] .'" ';
  $output .= 'value="'. $element['#return_value'] .'" ';
  $output .= (check_plain($element['#value']) == $element['#return_value']) ? ' checked="checked" ' : ' ';
  $output .= drupal_attributes($element['#attributes']) .' />';

  if (isset($element['#title'])) {
    $output .= '<label class="l10n-string" for="'. $element['#id'] .'">'. $element['#title'] .'</label>';
  }

  return $output;
}

function theme_l10n_community_list_translation_list($element) {
  $output = '<ul>';
  foreach (element_children($element) as $child) {
    if (is_numeric($child) || $child == 'new') {
      $output .= drupal_render($element[$child]);
    }
  }
  $output .= '</ul>';

  return $output;
}

function theme_l10n_community_list_source($element) {
  $output  = '<label class="l10n-string">'. drupal_render($element['source']) .'</label>';
  $output .= '<div class="l10n-usage"><a href="'. url('translate/details/'. $element['#langcode'] .'/'. $element['#string']->sid) .'" class="l10n-more">'. t('More information') .'</a></div>';
  return $output;
}

function theme_l10n_community_list_table($element) {
  $header = array(
    t('Source Text'),
    t('Translations'),
  );

  $rows = array();
  foreach (element_children($element) as $key) {
    $rows[] = array(
      array('class' => 'source', 'data' => theme('l10n_community_list_source', $element[$key])),
      array('class' => 'translation', 'data' => theme('l10n_community_list_translation_list', $element[$key])),
    );
  }


  return theme('table', $header, $rows, array('class' => 'l10n-table'));
}


function l10n_community_list_submit($form, &$form_state) {
  $declined = 0;
  $approved = 0;
  $inserted = 0;
  // $stabilized = 0;

  $langcode = $form_state['values']['langcode'];

  foreach ($form_state['values']['strings'] as $sid => $string) {
    foreach ($string as $tid => $options) {
      // Store new suggestion.
      $empty_values = 0;
      if (isset($options['value']) && is_array($options['value'])) {
        foreach ($options['value'] as $key => $value) {
          if ($value === t('<New translation>')) {
            $options['value'] = '';
            $empty_values++;
          }
        }
        if ($tid === 'new' && count($options['value']) > $empty_values) {
          $tid = _l10n_community_list_add_suggestion($langcode, $sid, $options['value']);
          if ($tid) {
            $inserted++;
            if ($string['active'] === 'new') {
              $string['active'] = $tid;
            }
          }
        }
      }

      if (is_numeric($tid) && $tid > 0) {
        if ($tid == $string['active']) {
          if ($options['original']->is_suggestion) {
            _l10n_community_list_approve_string($langcode, $sid, $tid);
            $approved++;
          }
          // if (/*!$options['original']->is_stable && */!empty($options['stable'])) {
          //   dsm('mark as stable');
          //   $stabilized++;
          // }
        }
        elseif (!empty($options['declined'])) {
          // also remove stable flag!
          $declined++;
          _l10n_community_list_decline_string($langcode, $sid, $tid);
        }
      }
    }
  }

  l10n_community_list_save_message($declined, $approved, $inserted);
}

function l10n_community_list_save_message($declined, $approved, $inserted) {
  $messages = array();
  if ($declined)
    $messages[] = format_plural($declined, '1 translation declined', '@count translations declined');
  if ($approved)
    $messages[] = format_plural($approved, '1 translation approved', '@count translations approved');
  if ($inserted)
    $messages[] = format_plural($inserted, '1 translation added', '@count translations added');

  if ($messages)
    drupal_set_message(implode(', ', $messages));
}


function _l10n_community_list_mock_translation($langcode, $sid) {
  if (!db_result(db_query("SELECT COUNT(*) FROM {l10n_community_translation} WHERE sid = %d AND language = '%s' AND is_suggestion = 0 AND is_active = 1", $sid, $langcode))) {
    // Insert mock tuple that acts as placeholder.
    db_query("INSERT INTO {l10n_community_translation} (sid, translation, language, uid_entered, time_entered, is_suggestion, is_active) VALUES (%d, '', '%s', 0, 0, 0, 1)", $sid, $langcode);
  }
}

function _l10n_community_list_add_suggestion($langcode, $sid, $translation) {
  global $user;

  // Load source string and adjust translation whitespace based on source.
  $source_string = db_result(db_query('SELECT value FROM {l10n_community_string} WHERE sid = %d', $sid));
  $translation = _l10n_community_list_pack_string($translation);
  $translation = l10n_community_trim($translation, $source_string);

  // Don't store empty translations.
  if ($translation === '') {
    return NULL;
  }

  // Look for an existing active translation, if any.
  $existing = db_fetch_object(db_query("SELECT tid FROM {l10n_community_translation} WHERE sid = %d AND language = '%s' AND translation = '%s'", $sid, $langcode, $translation));
  if (!empty($existing)) {
    // The translation is already in the db. Make it an active suggestion again.
    db_query("UPDATE {l10n_community_translation} SET is_suggestion = 1, is_active = 1 WHERE tid = %d", $existing->tid);
    $tid = $existing->tid;
  }
  else {
    // This is a new translation.
    _l10n_community_list_mock_translation($langcode, $sid);
    // Insert the new suggestion.
    db_query("INSERT INTO {l10n_community_translation} (sid, translation, language, uid_entered, time_entered, is_suggestion, is_active) VALUES (%d, '%s', '%s', %d, %d, 1, 1)", $sid, $translation, $langcode, $user->uid, time());
    $tid = db_last_insert_id('l10n_community_translation', 'tid');
  }

  // Mark the existing or mock translation has having suggestions.
  _l10n_community_list_update_suggestion_status($langcode, $sid);

  return $tid;
}

function _l10n_community_list_decline_string($langcode, $sid, $tid) {
  // Mark this translation as inactive.
  db_query("UPDATE {l10n_community_translation} SET is_suggestion = 0, is_active = 0 WHERE tid = %d", $tid);

  // Make sure the mock translation is there in case we declined the active translation.
  _l10n_community_list_mock_translation($langcode, $sid);
  _l10n_community_list_update_suggestion_status($langcode, $sid);
}

function _l10n_community_list_update_suggestion_status($langcode, $sid) {
  // Let's see if we have any suggestions remaining in this language.
  $count = db_result(db_query("SELECT COUNT(*) FROM {l10n_community_translation}
      WHERE sid = %d AND is_suggestion = 1 AND is_active = 1 AND language = '%s'", $sid, $langcode));

  // Update the status according to the number of suggestions.
  db_query("UPDATE {l10n_community_translation} SET has_suggestion = %d
      WHERE sid = %d AND is_suggestion = 0 AND is_active = 1 AND language = '%s'", $count ? 1 : 0, $sid, $langcode);
}

function _l10n_community_list_approve_string($langcode, $sid, $tid) {
  global $user;

  // Remove placeholder translation record (which was there if
  // first came suggestions, before an actual translation).
  db_query("DELETE FROM {l10n_community_translation} WHERE sid = %d AND translation = '' AND language = '%s'", $sid, $langcode);

  // Make the existing approved string a suggestion.
  db_query("UPDATE {l10n_community_translation} SET is_suggestion = 1 WHERE sid = %d AND language = '%s' AND is_suggestion = 0 AND is_active = 1", $sid, $langcode);

  // Mark this exact suggestion as active, and set approval time.
  db_query("UPDATE {l10n_community_translation} SET time_approved = %d, uid_approved = %d, is_suggestion = 0, is_active = 1 WHERE tid = %d;", time(), $user->uid, $tid);
  _l10n_community_list_update_suggestion_status($langcode, $sid);
}



function _l10n_community_list_unpack_string($string) {
  return explode("\0", $string);
}

function _l10n_community_list_pack_string($strings) {
  return implode("\0", $strings);
}



function l10n_community_get_language($langcode) {
  $languages = l10n_community_get_languages();
  return $languages[$langcode];
}




function _l10n_community_get_suggestions($string, $language) {
  $result = db_query("SELECT t.tid, t.sid, t.translation, t.uid_entered, t.time_entered, t.is_active, t.is_suggestion, u.name as username FROM {l10n_community_translation} t LEFT JOIN {users} u ON u.uid = t.uid_entered WHERE t.language = '%s' AND t.sid = %d AND t.is_active = 1 AND t.is_suggestion = 1 ORDER BY t.time_entered", $language->language, $string->sid);

  $suggestions = array();
  while ($suggestion = db_fetch_object($result)) {
    $suggestion->translation = _l10n_community_list_unpack_string($suggestion->translation);
    $suggestions[$suggestion->tid] = $suggestion;
  }
  return $suggestions;
}
