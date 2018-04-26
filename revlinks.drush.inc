<?php

/**
 * Implements hook_drush_command().
 *
 * {@inheritdoc}
 */
function revlinks_drush_command() {
  $items  = array();
  $items['revlinks'] = array(
    'callback'    => 'revlinks_batch_start_drush',
    'description' => t('Parse revlinks (reverse links)'),
    'arguments'   => [],
    'options' => [
      'verbose'    => 'Drush setting to show watchdog/error messages in command line',
    ],
  );
  return $items;
}

function revlinks_drush_help($section) {
  switch ($section) {
    case 'drush:revlinks':
      return t("Run Revlinks parser.");
  }
}
