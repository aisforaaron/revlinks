<?php

/**
 * @file Admin form settings and data output for testing
 */

require_once('revlinks.api.inc');
require_once('revlinks.module');
require_once('revlinks.sql.inc');

/**
 * Test method to print parser info to admin only
 *
 * Get first three pages from data source and parse,
 * or pass in absolute url. Nothing is saved to db.
 */
function revlinks_test() {
  $url = trim(variable_get('revlinks_test_url', ''));
  if (!empty($url)) {
    $links = revlinks_get_links_on_page($url);
    $message = 'Links parsed from test page :'
      . '<br>' . $url
      . '<br><pre>'
      . print_r($links, TRUE)
      . '</pre>';
    drupal_set_message($message);
  }
  else {
    $entityValues = revlinks_format_entities_array(
      revlinks_get_pages(0,3,TRUE),
      0,
      variable_get('revlinks_batch_amount', 10)
    );
    $message = 'Entity values from three parsed pages: <br><i>revlink_nid will be empty in testing</i>'
      . '<br><pre>'
      . print_r($entityValues, TRUE)
      . '</pre>';
    drupal_set_message($message);
  }
}

/**
 * Get some records for admin user to see results.
 *
 * @return array
 */
function revlinks_get_limited_report() {
  $output = [];
  // If records in parsed table, print a few links here
  if (!empty(revlinks_get_parse_types())) {
    $pages = _revlinks_sql_get_revlinks_sample();
    $linkCounter = 0;
    foreach ($pages as $page) {
      // Skip links to same page,
      // empty pages,
      // image links,
      // pagers.
      if(!empty(trim($page->link_url))
        && !empty(trim($page->link_title))
        && substr(trim($page->link_title), 0, 4) != '<img'
        && !strstr($page->link_url, 'page'))
      {
        $output[] = revlinks_format_report_link($page);
        $linkCounter++;
      }
      // Only display first ten that validate
      if($linkCounter == 10) {
        break;
      }
    }
  }
  return $output;
}

/**
 * Get report of pages to parse.
 */
function revlinks_get_limited_pages_report() {
  $output = [];
  // @todo make sure method gets expected results if not using fetchAllAssoc
  $pages = _revlinks_sql_get_updated_node_pages();
  $counter = 0;
  foreach ($pages as $page) {
    $output[] = '<a href="/' . $page->path . '">' . strip_tags($page->title) . '</a>';
    $counter++;
    if ($counter == 20) {
      break;
    }
  }
  return $output;
}

/**
 * Statistics/links notes on custom admin page tab.
 *
 * @throws Exception on theme()
 *
 * @return array
 */
function revlinks_get_reports_tab() {
  $content = 'Revlinks (short for reverse links) will tell you '
    . 'what internal pages link to a given node by scraping the '
    . 'published nodes on scheduled cron runs.';

  // Stats

  // Total pages in data source table
  $noConfigMsg = empty(variable_get('revlinks_node_types_to_parse', 0)) ? ' - you need to save the form first.' : '';
  $totalPagesToParse = revlinks_get_page_count();
  $totalPagesMsg = empty($totalPagesToParse) ? 'no pages ready to parse with current settings' . $noConfigMsg : $totalPagesToParse;
  // Total crons needed
  $numCrons = revlinks_get_total_cron_runs();
  $numCronsMsg = !empty($numCrons) ? $numCrons : 'You have already run all crons or you need to save the form first.';
  $infoStats = [
    'title' => 'Reports',
    'items'=> [
      'Total links parsed from all pages: ' . revlinks_get_links_parsed_count(),
      'Total pages module can parse, not counting homepage: ' . $totalPagesMsg,
      'Total crons needed if running over multiple crons: ' . $numCronsMsg
    ]
  ];
  // Add stat if debug mode enabled
  if (variable_get('revlinks_debug_mode', 0)) {
    $infoStats['items'][] = 'Debug stat: Total pages checked for links: ~' . variable_get('revlinks_total_pages_parsed', 0)
      . ' <br>(Some pages could be counted twice depending on publication workflow, so left as debug to get a rough idea of pages parsed.)';
  }
  $content .= theme('item_list', $infoStats);

  // Admin links

  $infoAdmin = [
    'title' => 'Admin Links',
    'items' => [
      l('Drupal Cron Page', '/admin/config/system/cron'),
      '<a href="revlinks/settings?reset=yes" 
        onclick="return confirm(\'Are you sure? This deletes your values below and loads the defaults.\')">
        Reset configuration</a> 
        This deletes all your saved settings. 
        You\'ll need to save the settings form after reset.',
      '<a href="revlinks/settings?clear=yes" 
        onclick="return confirm(\'Are you sure? This empties your parsed links table.\')">
        Clear data, counter and set mode to default</a>'
    ]
  ];
  $content .= theme('item_list', $infoAdmin);

  // Samples

  $samples = revlinks_get_limited_report();
  $infoSamples = [
    'title' => 'Sample Report',
    'items' => !empty($samples) ? $samples : ['No links available, please run report first.']
  ];
  $content .= theme('item_list', $infoSamples);

  // Update Mode Report

  $pageRecords = revlinks_get_limited_pages_report();
  $pages = [
    'title' => 'First twenty pages to parse in update mode',
    'items' => !empty($pageRecords) ? $pageRecords : ['No pages to parse yet.']
  ];
  $content .= theme('item_list', $pages);

  // Help Notes

  $infoNotes = [
    'items' => [
      'Data collected for "Revlinks" are shown on selected node edit screens for users with view reverse links access.',
      'The data collection should be run on off peak hours to avoid interfering with other processes.',
      'Since the parsing uses output HTML, it\'s best to have site caching enabled and warmed before running.',
      'The report can be scheduled or force run using Elysia Cron, or you can create a custom cron task to run "drush revlinks" which uses the Drupal batch api to process large data sets.',
      'Update mode will only parse pages that you have published after enabling update mode here. Also, in update mode when you run out of pages to parse, it will just re-parse the homepage if you have that enabled, on subsequent crons.',
      'The "Sample Report" may print links to pages you are not tracking if you don\'t select all content types to parse/view. This is expected behavior to collect all data and only report what is configured.']];
  $notes = theme('item_list', $infoNotes);
  // Add to collapsed fieldset
  $infoNotesBox = [
    'element' => [
      '#title' => t('Help Notes'),
      '#value' => $notes,
      '#children' => '<div>',
      '#collapsible' => TRUE,
      '#attributes' => ['class' => ['collapsible','collapsed']]
      ]
    ];
  drupal_add_library('system', 'drupal.collapse');
  $content .= theme('fieldset', $infoNotesBox);

  return ['#markup' => $content];
}

/**
 * Admin settings form.
 */
function revlinks_get_settings_tab() {
  global $base_url;
  drupal_set_title('Revlinks');
  // Admin form operations passed via GET
  if (isset($_GET['reset']) && trim($_GET['reset']) == 'yes') {
    // Reset vars to defaults
    revlinks_delete_variables();
    drupal_set_message('Default configuration has been loaded on the form below. Please make any edits and save the form now.');
    drupal_goto(revlinks_get_admin_config_path());
  }
  elseif (isset($_GET['clear']) && trim($_GET['clear']) == 'yes') {
    // Empty tables and reset some vars
    revlinks_delete_entities();
    _revlinks_sql_delete_pages([], TRUE); // update mode pages saved
    variable_set('revlinks_update_mode', 0);
    variable_set('revlinks_total_pages_parsed', 0);
    variable_set('revlinks_first_run_date', 0);
    variable_set('revlinks_last_run_date', 0);
    variable_set('revlinks_last_parsed_nid', 0);
    drupal_set_message('Data tables are truncated, counters reset, and update mode disabled.');
    drupal_goto(revlinks_get_admin_config_path());
  }
  elseif (isset($_GET['test']) && trim($_GET['test']) == 'yes') {
    // Running Tests
    revlinks_test();
    drupal_goto(revlinks_get_admin_config_path());
  }
  $form = array();

  // Testing

  $form['testing'] = array(
    '#type' => 'fieldset',
    '#title' => 'Testing',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  // Debug mode title/checkbox
  $form['testing']['debug_mode_help'] = array(
    '#type' => 'markup',
    '#markup' => '<b>Enable debug mode?</b>',
  );
  $form['testing']['revlinks_debug_mode'] = array(
    '#type' => 'checkbox',
    '#title' => t('Yes'),
    '#default_value' => variable_get('revlinks_debug_mode', 0),
    '#required' => FALSE,
    '#description' => t('Enable Drupal debug messages for module testing. This is mainly used for local development and not for production.'),
  );
  $form['testing']['info'] = array(
    '#type' => 'markup',
    '#markup' => 'Steps: <ol><li>Optionally add a url here, save the form and come back to this section <li>Click "Run Test" link which reloads the page with parser messages</ol>',
  );
  $form['testing']['revlinks_test_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Pass a url to test on parser'),
    '#description' => t('This is an absolute url from this domain to run through the parser to see what links would be added/skipped.'),
    '#size' => 80,
    '#maxlength' => 255,
    '#default_value' => variable_get('revlinks_test_url', ''),
    '#required' => FALSE,
  );
  // If this is your first time to the form and haven't saved it, the test parser link won't work, so hiding it
  if (empty(variable_get('revlinks_node_types_to_parse', ''))) {
    $parserMsg = 'You need to save the form before running tests here.';
  }
  else {
    $parserLinkMsg = empty(variable_get('revlinks_test_url', '')) ? 'No link saved yet, will run first three results.' : 'Link is saved, will parse it.';
    $parserMsg = 'The below link will query the data source table for three records OR use the test parser url you previously saved to this form, and run them through the link parser in this module. You can then see a Drupal message of the data found. This will not be saved to the database.<br><br><a href="?test=yes">Run Parser Test >></a> (' . $parserLinkMsg . ')</li>';
  }
  $form['testing']['run_test'] = array(
    '#type' => 'markup',
    '#markup' => $parserMsg,
  );

  // Batch and Cron Amounts

  $form['amounts'] = array(
    '#type' => 'fieldset',
    '#title' => 'Batch & Cron Amounts',
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $form['amounts']['revlinks_batch_amount'] = array(
    '#type' => 'textfield',
    '#title' => t('Amount of pages to parse per batch'),
    '#size' => 3,
    '#maxlength' => 3,
    '#default_value' => variable_get('revlinks_batch_amount', 10),
    '#required' => TRUE,
  );
  $form['amounts']['revlinks_batch_max'] = array(
    '#type' => 'textfield',
    '#title' => t('Max count of pages in site you want to parse'),
    '#description' => t('Enter the text <b>"all"</b> without quotes to parse all pages.'),
    '#size' => 5,
    '#maxlength' => 5,
    '#default_value' => variable_get('revlinks_batch_max', 'all'),
    '#required' => TRUE,
  );
  $form['amounts']['revlinks_batches_per_cron'] = array(
    '#type' => 'textfield',
    '#title' => t('Amount of batches to run per cron'),
    '#description' => t('If you want to parse pages over multiple cron runs you can set the amount per cron here.<br> The module will pickup on the next cron starting after the last parsed nid page. <br>So if you have 10 pages per batch, "all" pages set to parse and 5 batches per cron run, <br>you will parse 50 pages per cron run until you run through all the available pages to parse.'),
    '#size' => 5,
    '#maxlength' => 5,
    '#default_value' => variable_get('revlinks_batches_per_cron', 5),
    '#required' => TRUE,
  );

  // General

  $form['general'] = array(
    '#type' => 'fieldset',
    '#title' => 'General',
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  // Update mode title/checkbox
  $form['general']['update_mode_help'] = array(
    '#type' => 'markup',
    '#markup' => '<b>Enable update mode?</b>',
  );
  $form['general']['revlinks_update_mode'] = array(
    '#type' => 'checkbox',
    '#title' => t('Yes'),
    '#default_value' => variable_get('revlinks_update_mode', 0),
    '#required' => FALSE,
    '#description' => t('Set the Revlinks module to only parse nodes that have been published after the last time the parser ran, which was ' . revlinks_get_last_run_date_readable() . '.<br>This will append records to the table instead of rebuilding the data.'),
  );
    // Frontpage title/checkbox
  $form['general']['frontpage_help'] = array(
    '#type' => 'markup',
    '#markup' => '<b>Parse the homepage?</b>',
  );
  $form['general']['revlinks_parse_frontpage'] = array(
    '#type' => 'checkbox',
    '#title' => t('Yes'),
    '#default_value' => variable_get('revlinks_parse_frontpage', 1),
    '#required' => FALSE,
  );
    // Local anchors title/checkbox
  $form['general']['anchor_help'] = array(
    '#type' => 'markup',
    '#markup' => '<b>Skip anchor tags?</b>',
  );
  $form['general']['revlinks_parse_anchors'] = array(
    '#type' => 'checkbox',
    '#title' => t('Yes'),
    '#default_value' => variable_get('revlinks_parse_anchors', 1),
    '#required' => FALSE,
  );
  $form['general']['revlinks_base_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Base url of site to parse, no trailing slash'),
    '#description' => t('If you are running this on one server but want to check another with the same nodes add the base url here.'),
    '#size' => 80,
    '#maxlength' => 255,
    '#default_value' => variable_get('revlinks_base_url', $base_url),
    '#required' => TRUE,
  );

  // CSS Related

  $form['styles'] = array(
    '#type' => 'fieldset',
    '#title' => 'CSS and Styles',
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $form['styles']['revlinks_main_class'] = array(
    '#type' => 'textfield',
    '#title' => t('Main class parser will use to find content body'),
    '#description' => t('Add an html attribute and css class'),
    '#size' => 80,
    '#maxlength' => 255,
    '#default_value' => variable_get('revlinks_main_class', 'div.region-content'),
    '#required' => FALSE,
  );
  $form['styles']['revlinks_skip_classes'] = array(
    '#type' => 'textfield',
    '#title' => t('Optional list of classes parser should skip'),
    '#description' => t('Add a space separated list of class names that should not be saved to the report. <br>This could be template links you have on every page that are inside the main content area like breadcrumbs.'),
    '#size' => 80,
    '#maxlength' => 255,
    '#default_value' => variable_get('revlinks_skip_classes', 'skipclass page-header--alt-label tabindex'),
    '#required' => FALSE,
  );

  // Content Types

  $form['content_types'] = array(
    '#type' => 'fieldset',
    '#title' => 'Content types',
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $form['content_types']['revlinks_node_types_to_parse'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Select content types to parse'),
    '#description' => t('You should probably not alter these settings if you have already started parsing the site. Results may be unpredictable regarding what is parsed or skipped in the queue. Best practice is to use the "Clear data and counter" link here to start over.'),
    '#options' => drupal_map_assoc(array_keys(node_type_get_types())),
    '#default_value' => variable_get(
      'revlinks_node_types_to_parse', 
      []
    ),
    '#required' => TRUE,
  );
  $form['content_types']['revlinks_node_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Select content types that should display reverse links report tab'),
    '#options' => drupal_map_assoc(array_keys(node_type_get_types())),
    '#default_value' => variable_get(
      'revlinks_node_types',
      []
    ),
    '#required' => TRUE,
  );

  return system_settings_form($form);
}
