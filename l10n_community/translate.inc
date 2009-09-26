<?php
// $Id: translate.inc,v 1.1.2.7.2.19 2009/09/18 18:03:19 goba Exp $

/**
 * @file
 *   Translation view and editing pages for localization community.
 */

// = Filter form handling ======================================================

/**
 * Translate form filter.
 *
 * @param $filters
 *   Array of filter options.
 * @param $limited
 *   Return limited set of filters (no suggestion filters).
 */
function l10n_community_filter_form(&$form_state, $filters, $limited = FALSE) {
  $projects = l10n_community_get_projects();

  $translation_options = array(
    L10N_STATUS_ALL            => t('<Any>'),
    L10N_STATUS_UNTRANSLATED   => t('Untranslated'),
    L10N_STATUS_TRANSLATED     => t('Translated'),
  );
  $suggestion_options = array(
    L10N_STATUS_ALL            => t('<Any>'),
    L10N_STATUS_NO_SUGGESTION  => t('Has no suggestion'),
    L10N_STATUS_HAS_SUGGESTION => t('Has suggestion'),
  );
  
  $form['project'] = array(
    '#title' => t('Project'),
    '#default_value' => isset($filters['project']) ? $filters['project']->title : '',
  );
  if (($count = count($projects)) <= 30) {
    // Select widget for 1-30 projects.
    $form['project']['#type'] = 'select';
    $form['project']['#options'] = array('' => t('All'));
    foreach ($projects as $project) {
      // URI used to shorten the lookup cycle in filter sanitization.
      $form['project']['#options'][$project->uri] = $project->title;
    }
  }
  else {
    // Autocomplete field for more then 30 projects.
    $form['project'] += array(
      '#type' => 'textfield',
      '#autocomplete_path' => 'translate/projects/autocomplete',
      '#size' => 20,
    );
  }

  if (isset($filters['project'])) {
    $releases = l10n_community_get_releases($filters['project']->uri);
    $release_options = array('all' => t('All'));
    foreach ($releases as $rid => $release) {
      $release_options[$rid] = $release->title;
    }
    $form['release'] = array(
      '#title' => t('Release'),
      '#type' => 'select',
      '#options' => $release_options,
      '#default_value' => isset($filters['release']) ? $filters['release'] : 'all',
    );
  }

  if (count($contexts = l10n_community_get_contexts()) > 1) {
    $form['context'] = array(
     '#title' => t('Context'),
      '#type' => 'select',
      '#options' => array('all' => t('All')) + $contexts,
      '#default_value' => $filters['context'],
    );
  }

  $form['status'] = array(
    '#title' => t('Status'),
    '#tree' => TRUE,
  );
  $form['status']['translation'] = array(
    '#type' => 'select',
    '#options' => $translation_options,
    '#default_value' => $filters['status'] & (L10N_STATUS_TRANSLATED | L10N_STATUS_UNTRANSLATED),
  );
  if (!$limited) {
    $form['status']['suggestion'] = array(
      '#type' => 'select',
      '#options' => $suggestion_options,
      '#default_value' => $filters['status'] & (L10N_STATUS_HAS_SUGGESTION | L10N_STATUS_NO_SUGGESTION),
    );
  }

  $form['author']= array(
    '#type' => 'textfield',
    '#title' => t('Submitted by'),
    '#maxlength' => 60,
    '#autocomplete_path' => 'user/autocomplete',
    '#default_value' => isset($filters['author']) ? $filters['author']->name : '',
    '#size' => 15,
  );

  $form['search'] = array(
    '#title' => t('Contains'),
    '#type' => 'textfield',
    '#default_value' => $filters['search'],
    '#size' => 20,
  );

  $form['limit'] = array(
    '#type' => 'select',
    '#title' => t('Limit'),
    '#default_value' => isset($filters['limit']) ? $filters['limit'] : 10,
    '#options' => drupal_map_assoc(array(5, 10, 20, 30)),
    '#default_value' => $filters['limit'],
  );

  $form['submit'] = array(
    '#value' => t('Filter'),
    '#type' => 'submit',
  );
  $form['reset'] = array(
    '#value' => t('Reset'),
    '#type' => 'submit',
  );
  $form['#theme'] = 'l10n_community_filter_form';
  return $form;
}

/**
 * Submission handler for filtering form.
 */
function l10n_community_filter_form_submit($form, &$form_state) {

  if ($form_state['values']['op'] == t('Reset')) {
    // Just go with the redirection flow => removes URL params.
    return;
  }

  if ($form_state['values']['op'] == t('Filter')) {
    $filters = l10n_community_build_filter_values($form_state['values']);
    // Replace some values by their string representation.
    foreach (array('project' => 'uri', 'author' => 'name') as $name => $key) {
      if (!empty($filters[$name])) {
        $filters[$name] = $filters[$name]->$key;
      }
    }

    // Redirect keeping the relevant filters intact in the URL.
    $form_state['redirect'] = array($_GET['q'], $filters);
  }
}

/**
 * Theme function for l10n_community_filter_form.
 */
function theme_l10n_community_filter_form($form) {
  $row = array();
  $labels = array();
  // Only display these elements in distinct table cells
  $elements = array('project', 'release', 'context', 'status', 'author', 'search', 'limit');
  foreach ($form as $id => &$element) {
    if (in_array($id, $elements)) {
      $labels[] = $element['#title'];
      unset($element['#title']);
      $row[] = drupal_render($element);
    }
  }
  // Fill in the rest of the header above the buttons.
  $labels[] = '';
  // Display the rest of the form in the last cell
  $row[] = array('data' => drupal_render($form), 'class' => 'last');
  return theme('table', $labels, array($row), array('class' => 'l10n-server-filter'));
}

// = Miscellaneous =============================================================

/**
 * Theme context information for source strings.
 *
 * @param $string
 *   Source string object (based on l10n_community_string columns).
 */
function theme_l10n_community_in_context($string) {
  if (!empty($string->context)) {
    return '<div class="string-context">'. t('in context: @context', array('@context' => $string->context)) .'</div>';
  }
  return '';
}

// = API functions =============================================================

/**
 * Get strings under some conditions.
 *
 * @param $langcode
 *   Language code to use for the lookup.
 * @param $filters
 *   - 'project'
 *     Project object to look up strings for.
 *   - 'status'
 *     Filter strings by status. See L10N_STATUS_ALL,
 *     L10N_STATUS_UNTRANSLATED, L10N_STATUS_HAS_SUGGESTION and
 *     L10N_STATUS_TRANSLATED.
 *   - 'release'
 *     Release id of the particular project release to filter with.
 *     Use NULL to not filter on releases.
 *   - 'search'
 *     Substring to search for in all source and translation strings.
 *   - 'context'
 *     From Drupal 7, separate contexts are supported. POTX_CONTEXT_NONE is
 *     the default, if the code does not specify a context otherwise.
 * @param $pager
 *   Number of strings to be returned in a pager. Should be NULL if
 *   no pager should be used.
 * @return
 *   An array of string records from database.
 */
function l10n_community_get_strings($langcode, $filters, $pager = NULL) {
  $join = $join_args = $where = $where_args = array();
  $sql = $sql_count = '';

  $select = "SELECT DISTINCT s.sid, s.value, s.context, t.tid, t.language, t.translation, t.uid_entered, t.uid_approved, t.time_entered, t.time_approved, t.has_suggestion, t.is_suggestion, t.is_active, u.name as username, u2.name as username_approved FROM {l10n_community_string} s";
  $select_count = "SELECT COUNT(DISTINCT(s.sid)) FROM {l10n_community_string} s";
  $join[] = "LEFT JOIN {l10n_community_translation} t ON s.sid = t.sid AND t.language = '%s' AND t.is_active = 1 AND t.is_suggestion = 0";
  $join_args[] = $langcode;
  $join[] = "LEFT JOIN {users} u ON u.uid = t.uid_entered";
  $join[] = "LEFT JOIN {users} u2 ON u2.uid = t.uid_approved";

  // Add submitted by condition
  if (!empty($filters['author'])) {
    $where[] = "t.uid_entered = %d";
    $where_args[] = $filters['author']->uid;
  }

  // Release restriction.
  $release = empty($filters['release']) || $filters['release'] === 'all' ? NULL : $filters['release'];
  $project = $filters['project'];
  if ($release || $project) {
    $join[] = "INNER JOIN {l10n_community_line} l ON s.sid = l.sid";
    // If we have a release we ignore the project
    if ($release) {
      // Release restriction.
      $where_args[] = $release;
      $where[] = 'l.rid = %d';
    }  
    elseif ($project) {
      $where[] = "l.pid = %d";
      $where_args[] = $project->pid;
    }
  }

  // Context based filtering.
  if (isset($filters['context']) && $filters['context'] != 'all') {
    // We use 'none' for no context, so '' can be the defaut (for all contexts).
    $where_args[] = $filters['context'] == 'none' ? '' : $filters['context'];
    $where[] = "s.context = '%s'";
  }

  if (!empty($filters['search'])) {
    // Search in the source or target strings.
    $where_args[] = $filters['search'];
    $where_args[] = $filters['search'];
    $where[] = "(s.value LIKE '%%%s%%' OR t.translation LIKE '%%%s%%')";
  }

  // Restriction based on string status by translation / suggestions.
  $status_sql = '';
  if (isset($filters['status']) && $filters['status'] & L10N_STATUS_UNTRANSLATED) {
    // We are doing a LEFT JOIN especially to look into the case, when we have nothing
    // to match in the translation table, but we still have the string. (We get our
    // records in the result set in this case). The translation field is empty or
    // NULL in this case, as we are not allowing NULL there and only saving an empty
    // translation if there are suggestions but no translation yet.
    $where[] = "(t.translation is NULL OR t.translation = '')";
  } 
  elseif ($filters['status'] & L10N_STATUS_TRANSLATED) {
    $where[] = "t.translation != ''";
  }
  if (isset($filters['status']) && $filters['status'] & L10N_STATUS_HAS_SUGGESTION) {
    // Note that we are not searching in the suggestions themselfs, only
    // the source and active translation values. The user interface underlines
    // that we are  looking for strings which have suggestions, not the
    // suggestions themselfs.
    $where[] = "t.has_suggestion = 1";
  }
  elseif ($filters['status'] & L10N_STATUS_NO_SUGGESTION) {
    $where[] = "((t.has_suggestion IS NULL) OR (t.has_suggestion = 0))";
  }

  // Build the queries
  $sql_args = array_merge($join_args, $where_args);
  $sql_where = implode(' ', $join) . (count($where) ? (' WHERE '. implode(' AND ', $where)) : '');
  $sql = $select .' '. $sql_where;
  $sql_count = $select_count .' '. $sql_where;

  // We either need a pager or a full result.
  if (isset($pager)) {
    $strings = pager_query($sql, $pager, 0, $sql_count, $sql_args);
  }
  else {
    $strings = db_query($sql, $sql_args);
  }
  $result = array();
  while ($string = db_fetch_object($strings)) {
    $result[] = $string;
  }
  return $result;
}

/**
 * Check and sanitize arguments and build filter array.
 *
 * @param $params
 *   Associative array with unsanitized values.
 * @param $suggestions
 *   Whether we build the filters for the suggestions (TRUE) or not (FALSE).
 */
function l10n_community_build_filter_values($params, $suggestions = FALSE) {
  $project = $release = NULL;

  // Convert array representation of flags to one integer.
  if (isset($params['status']) && is_array($params['status'])) {
    if (isset($params['status']['suggestion'])) {
      $params['status'] = ((int) $params['status']['translation']) | ((int) $params['status']['suggestion']);
    }
    else {
      $params['status'] = (int) $params['status']['translation'];
    }
  }

  $filter = array(
    'project' => NULL,
    'status' => isset($params['status']) ? (int) $params['status'] : L10N_STATUS_ALL,
    'release' => 'all',
    'search' => !empty($params['search']) ? (string) $params['search'] : '',
    'author' => !empty($params['author']) && ($account = user_load(array('name' => $params['author']))) ? $account : NULL,
    // Dropdown, validated by form API.
    'context' => isset($params['context']) ? (string) $params['context'] : 'all',
    'limit' => (isset($params['limit']) && in_array($params['limit'], array(5, 10, 20, 30))) ? (int) $params['limit'] : 10,
  );
  
  // The project can be a dropdown or text field depending on number of 
  // projects. So we need to sanitize its value. 
  if (isset($params['project'])) {
    // Try to load project by uri or title, but give URI priority. URI is used
    // to shorten the URL and have simple redirects. Title is used if the
    // filter form was submitted, but that one is simplified to the URI on
    // redirect to make the URL shorter.
    $project = l10n_community_get_projects(array('uri' => $params['project']));
    if (empty($project)) {
      $project = db_fetch_object(db_query("SELECT * FROM {l10n_community_project} WHERE title = '%s'", $params['project']));
    }
    if (!empty($project)) {
      $filter['project'] = $project;
      if (isset($params['release']) && ($releases = l10n_community_get_releases($project->uri)) && isset($releases[$params['release']])) {
        // Allow to select this release, if belongs to current project only.
        $filter['release'] = $params['release'];
      }
    }
  }
  return $filter;
}

// = AJAX callbacks ============================================================

/**
 * Return a HTML list of projects, releases and counts of where strings
 * appear in the managed projects.
 *
 * We could have been provided much more information, but usability should
 * also be kept in mind. It is possible to investigate hidden information
 * sources though, like tooltips on the release titles presented.
 *
 * This callback is invoked from JavaScript and is used as an AHAH provider.
 *
 * @param $langcode
 *   Language code.
 * @param $sid
 *   Source string id.
 */
function l10n_community_string_details($langcode = NULL, $sid = 0) {
  // Prevent devel module information.
  $GLOBALS['devel_shutdown'] = FALSE;

  // List of project releases, where this string is used.
  $result = db_query('SELECT l.pid, p.title project_title, l.rid, r.title release_title, COUNT(l.lineno) as occurance_count FROM {l10n_community_line} l INNER JOIN {l10n_community_project} p ON l.pid = p.pid INNER JOIN {l10n_community_release} r ON l.rid = r.rid WHERE l.sid = %d AND p.status = 1 GROUP BY l.rid ORDER BY l.pid, l.rid', $sid);

  $list = array();
  $output = array();
  $previous_project = '';
  while ($instance = db_fetch_object($result)) {
    if ($instance->project_title != $previous_project) {
      if (!empty($list)) {
        $output[] = join(', ', $list);
      }
      $list = array('<em>'. $instance->project_title .':</em> '. $instance->release_title .' ('. $instance->occurance_count .')');
    }
    else {
      $list[] = $instance->release_title .' ('. $instance->occurance_count .')';
    }
    $previous_project = $instance->project_title;
  }
  $output[] = join(', ', $list);
  $usage_info = '<strong>'. t('Used in:') .'</strong>'. theme('item_list', $output);

  // Information about translator and translation timestamp.
  $translation_info = '';
  $translation = db_fetch_object(db_query("SELECT translation, uid_entered, time_entered FROM {l10n_community_translation} WHERE language = '%s' AND sid = %d AND is_active = 1 AND is_suggestion = 0", $langcode, $sid));
  if (!empty($translation->translation)) {
    $account = user_load(array('uid' => $translation->uid_entered));
    $translation_info = t('<strong>Translated by:</strong><br /> %username at %date', array('%username' => $account->name, '%date' => format_date($translation->time_entered)));
  }

  print $usage_info . $translation_info; // . $suggestion_info;
  exit;
}

