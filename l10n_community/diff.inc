<?php
// $Id$

/**
 * @file
 *   Visualizes differences between translations.
 */

function _worddiff_tokenize($strings) {
  $tokens = $length = array();
  $prefix = $suffix = '';

  // Prepare the strings.
  foreach($strings as $key => $str) {
    // Tokenize the string
    $tokens[$key] = preg_split('/(?<=\W)|(?=\W)/u', trim($str));
  }

  // Remove identical prefix.
  while ($tokens[0] && $tokens[1] && reset($tokens[0]) === reset($tokens[1])) {
    $prefix .= array_shift($tokens[0]);
    array_shift($tokens[1]);
  }

  // Remove identical suffixes.
  while ($tokens[0] && $tokens[1] && end($tokens[0]) === end($tokens[1])) {
    $suffix = array_pop($tokens[0]) . $suffix;
    array_pop($tokens[1]);
  }

  // Calculate the length of the token arrays.
  foreach (array_keys($tokens) as $key) {
    $length[$key] = count($tokens[$key]);
  }

  return array($tokens, $length, $prefix, $suffix);
}

function _worddiff_lcs($tokens, $length) {
  $matrix = array();

  // Calculate the matrix.
  for ($i = 0; $i < $length[0]; $i++) {
    for ($j = 0; $j < $length[1]; $j++) {
      if ($tokens[0][$i] === $tokens[1][$j]) {
        $matrix[$i][$j] = (isset($matrix[$i - 1][$j - 1]) ? $matrix[$i - 1][$j - 1] : 0) + 1;
      }
      else {
        $matrix[$i][$j] = max(
          isset($matrix[$i][$j - 1]) ? $matrix[$i][$j - 1] : 0,
          isset($matrix[$i - 1][$j]) ? $matrix[$i - 1][$j] : 0
        );
      }
    }
  }

  return $matrix;
}

function _worddiff_expand_result($str) {
  return array(-1 => '', 0 => $str, 1 => '');
}

function _worddiff_changeset($tokens, $matrix, $length) {
  // Setup work.
  $index = 0;
  $mode = NULL;
  $result = array_map('_worddiff_expand_result', $tokens[0]);

  for ($i = $length[0] - 1, $j = $length[1] - 1; $i >= 0; $i--, $j--) {
    if ($j < 0 || $tokens[0][$i] !== $tokens[1][$j]) {
      if ($j < 0 || ($j > 0 && ($matrix[$i][$j - 1] < $matrix[$i - 1][$j]))) {
        // Delete substring.
        if ($mode === -1) {
          // If we're already in delete mode, prepend it to the previous delete.
          $result[$index][-1] = $result[$i][0] . $result[$index][-1];
          unset($result[$i]);
        }
        else {
          // Otherwise, move this token's string to the delete part.
          $result[$i][-1] = $result[$i][0];
          $result[$i][0] = '';
          $index = $i;
          $mode = -1;
        }
        $j++;
      }
      else {
        // Add a new substring to the last token.
        $result[$i][1] = $tokens[1][$j] . $result[$i][1];
        $i++;
        $mode = 1;
      }
    }
    else {
      // Preserve substring.
      if ($mode === 0) {
        // Merge this token into the previous one to minimize the amount of tokens.
        $result[$index][0] = $result[$i][0] . $result[$index][0];
        unset($result[$i]);
      }
      else {
        $index = $i;
        $mode = 0;
      }
    }
  }

  return array_values($result);
}

function _worddiff_find_prefix($a, $b) {
  $prefix = '';

  while (drupal_strlen($a) && drupal_strlen($b) && drupal_substr($a, 0, 1) === drupal_substr($b, 0, 1)) {
    $prefix .= drupal_substr($a, 0, 1);
    $a = drupal_substr($a, 1);
    $b = drupal_substr($b, 1);
  }

  return array($prefix, $a, $b);
}

function _worddiff_aggregate($diff) {
  for ($i = 0; isset($diff[$i]); $i++) {
    if ($diff[$i][-1] == '' && drupal_strlen($diff[$i][0]) < 3 && $diff[$i][1] == '' &&
        isset($diff[$i - 1]) && $diff[$i - 1][0] == '' &&
        isset($diff[$i + 1]) && $diff[$i + 1][0] == '') {
      $diff[$i + 1] = array(
        -1 => $diff[$i - 1][-1] . $diff[$i][0] . $diff[$i + 1][-1],
        0 => '',
        1 => $diff[$i - 1][1] . $diff[$i][0] . $diff[$i + 1][1],
      );

      unset($diff[$i - 1], $diff[$i]);
    }
  }

  // Find common prefixes.
  $diff = array_values($diff);
  for ($i = 0; isset($diff[$i]); $i++) {
    if ($diff[$i][0] == '' && isset($diff[$i - 1]) && $diff[$i - 1][1] == '') {
      list($prefix, $diff[$i][-1], $diff[$i][1]) = _worddiff_find_prefix($diff[$i][-1], $diff[$i][1]);
      $diff[$i - 1][0] .= $prefix;
    }
  }

  return $diff;
}

function worddiff($old, $new) {
  // Preparation stuff.
  list($tokens, $length, $prefix, $suffix) = _worddiff_tokenize(array($old, $new));
  $matrix = _worddiff_lcs($tokens, $length);

  // Generate the changeset.
  $result = _worddiff_changeset($tokens, $matrix, $length);

  // Add the previously trimmed prefix and suffix.
  array_unshift($result, array(-1 => '', 0 => $prefix, 1 => ''));
  array_push($result, array(-1 => '', 0 => $suffix, 1 => ''));

  // Merge small changes together.
  $result = _worddiff_aggregate($result);

  return $result;
}

function _worddiff_render_deletions($changes) {
  $value = '';

  if (drupal_strlen($changes[-1])) {
    $value .= '<del>'. $changes[-1] .'</del>';
  }
  if (drupal_strlen($changes[0])) {
    $value .= $changes[0];
  }

  return $value;
}

function _worddiff_render_insertions($changes) {
  $value = '';

  if (drupal_strlen($changes[0])) {
    $value .= $changes[0];
  }
  if (drupal_strlen($changes[1])) {
    $value .= '<ins>'. $changes[1] .'</ins>';
  }

  return $value;
}

function worddiff_render($result) {
  return array(
    'old' => implode('', array_map('_worddiff_render_deletions', $result)),
    'new' => implode('', array_map('_worddiff_render_insertions', $result))
  );
}

