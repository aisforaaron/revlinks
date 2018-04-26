<?php

/**
 * @file API methods
 *
 * All Drupal hooks are in .module file.
 */

use \Drupal\revlinks\LinkScraper;

require_once 'LinkScraper.class.php';


/**
 * Get data and reports
 */

/**
 * Helper method to print out debug statements in debug mode
 *
 * @param string $msg
 *   Message to print to debug area
 */
function _revdebug($msg = '') {
  if (variable_get('revlinks_debug_mode', 0)) {
    drupal_set_message('Revlinks Debug: <br>' . $msg);
  }
}

/**
 * Get the admin form path
 *
 * If this changes, update the install and readme files.
 *
 * @return string
 */
function revlinks_get_admin_config_path() {
  return '/admin/config/search/revlinks';
}

function revlinks_get_mode() {
  return !empty(variable_get('revlinks_update_mode', '')) ? 'update' : 'default';
}

/**
 * Get selected content types to parse
 *
 * @return array of strings
 */
function revlinks_get_parse_types() {
  return array_filter(variable_get('revlinks_node_types_to_parse', []));
}

/**
 * Get link options from config settings
 *
 * Obtain the data from the database and make calculations
 * Space delim list of href classes to skip
 * Convert to array of class names
 *
 * @return array
 */
function revlinks_get_options() {
  $skipClasses = variable_get('revlinks_skip_classes', '');
  if (!empty($skipClasses)) {
    $clean = explode(' ', trim($skipClasses));
    $skipClasses = array_filter($clean);
  }
  return [
    'mainClass'   => variable_get('revlinks_main_class', 'div.region-content'),
    'skipAnchors' => variable_get('revlinks_parse_anchors', 1),
    'skipClasses' => $skipClasses,
  ];
}

/**
 * Get last batch run datetime from timestamp
 *
 * @return string
 *   Textual representation of date as Month #, year, time OR 'never'.
 */
function revlinks_get_last_run_date_readable() {
  $lastRunDate = variable_get('revlinks_last_run_date', '');
  $lastRunMsg = !empty($lastRunDate) ? date('F j, Y, g:i a', $lastRunDate) : 'never';
  return $lastRunMsg;
}

/**
 * Get pages to parse and return data ready for entity create.
 *
 * Method will adjust node table query based on the mode set
 * in the admin config (update || default) by calling
 * _revlinks_sql_get_node_pages() for default
 * and
 * _revlinks_sql_get_updated_node_pages() for update.
 *
 * @param int $from
 *   Starting record position of result set
 *
 * @param mixed $count
 *   Number to limit results or default 'all' string
 *
 * @param bool $testFlag
 *   Flag to indicate running a test from admin page
 *
 * @return array of results
 */
function revlinks_get_pages($from = 0, $count = 'all', $testFlag = FALSE) {
  // Just testing parser from admin page
  if ($testFlag) {
    $results = _revlinks_sql_get_node_pages(['from' => $from, 'count' => $count]);
  }
  elseif (revlinks_get_mode() == 'update') {
    // Update mode
    $results = _revlinks_sql_get_updated_node_pages();
  }
  else {
    // Default mode
    if ($count != 'all') {
      // If parsing some, just get a set
      $results = _revlinks_sql_get_node_pages(['from' => $from, 'count' => $count]);
    }
    else {
      // Otherwise, get all pages for batches in one cron
      $results = _revlinks_sql_get_node_pages();
    }
  }
  return $results;
}

/**
 * Get form markup content to display reverse links data
 *
 * @param $node
 *
 * @throws Exception on revlinks_get_report for theme()
 *
 * @return array
 */
function revlinks_get_report_page($node) {
  if (in_array($node->type, revlinks_get_parse_types())) {
    $content = revlinks_get_report($node->nid);
  }
  else {
    $content = 'Report output not enabled for this content type.';
  }
  return ['#markup' => $content];
}

/**
 * Format a link for viewing in report.
 *
 * @param object $page
 *
 * @return string
 */
function revlinks_format_report_link($page) {
  // Page link was found
  // homepage or node page
  $url   = ($page->parsed_nid == 0) ? variable_get('site_frontpage', 'node') : $page->parsed_path;
  $title = ($page->parsed_nid == 0) ? 'Homepage' : strip_tags($page->parsed_title);
  $pageLinkWasFound = '<a href="/' . $url . '">' . $title . '</a>';
  // Link that was found on page
  // Print alt text for img if title is an img tag
  if (substr(trim($page->link_title), 0, 4) == '<img') {
    preg_match('/(alt=")(.*)(")/', $page->link_title, $matches);
    $imgAlt = !empty($matches[2]) ? $matches[2] : 'Image without alt tag';
    $linkOnPage = '<a href="/' . $page->link_url . '">Image link alt text: ' . $imgAlt . '</a>';
  }
  else {
    $linkOnPage = '<a href="/' . $page->link_url . '">' . strip_tags($page->link_title) . '</a>';
  }
  $output = $pageLinkWasFound . ' -- ' . $linkOnPage;
  return $output;
}

/**
 * Get report data to display
 *
 * @param int $nid node id
 *   Id of node page admin is viewing
 *
 * @throws Exception on theme()
 *
 * @return string
 *   HTML output for node edit. Links that point to current node id page.
 */
function revlinks_get_report($nid) {
  global $language;

  // Query reverse links data
  $node = node_load($nid);
  $configLink = user_access('administer revlinks') ? l('Config Page', revlinks_get_admin_config_path()) . '<br>' : '';
  $reverseLinks = '<h2>Revlinks Report</h2>'
    . 'All internal pages that have links to this page in their content body are shown here. '
    . 'If the link is around an image, you will see a link to the page and the alt text for that image. '
    . '<br>Report last run date: ' . revlinks_get_last_run_date_readable()
    . '<br>' . $configLink . '<br>';

  // Get path
  // Path is the link url scraped off the page
  $path = url('node/' . $node->nid, ['language' => $language]);
  // @todo make sure the replacement query here works
  // Get aliases for current page, no leading slash
  $aliases = _revlinks_sql_get_aliases($path);
  if (!empty($aliases)) {
    $aliases = array_keys($aliases);
  }
  // Get Redirects
  $redirects = '';
  if (module_exists('redirect')) {
    // @todo make sure the replacement redirects method works
    $redirects = _revlinks_sql_get_redirects($path);
    if (!empty($redirects)) {
      $redirects = array_keys($redirects);
    }
  }
  // @todo make sure $links method is correct
  // Build query to revlinks table to get matches
  $links = _revlinks_sql_get_revlinks($path, $aliases, $redirects);
  // For testing only
  if (variable_get('revlinks_debug_mode', 0)) {
    drupal_set_message('Revlinks Debug: Aliases <br>' . print_r($aliases, TRUE));
    drupal_set_message('Revlinks Debug: Redirects <br>' . print_r($redirects, TRUE));
    $output = [];
    foreach ($links as $link) {
      $output[] = $link->parsed_nid . ' | ' . $link->parsed_title;
    }
    drupal_set_message('Revlinks Debug: Revlinks <br>' . print_r($output, TRUE));
  }
  // Links for report
  $report = [];
  if (!empty($links)) {
    foreach ($links as $link) {
      $report[] = revlinks_format_report_link($link);
    }
    // Count duplicates (same links used multiple times on a page)
    $reportCounts = array_count_values($report);
    $rows = [];
    foreach ($reportCounts as $key => $value) {
      $parts = explode('--', $key);
      $rows[] = [$parts[0], $parts[1], $value];
    }
    // Put results into table
    $header = ['Page link was found', 'Link', 'Count'];
    $reverseLinks .= theme('table', ['header' => $header, 'rows' => $rows]);
  }
  else {
    $reverseLinks .= '<li>No links found</li>';
  }
  return $reverseLinks;
}

/**
 * Based on module vars, get amount of cron runs needed
 *
 * This checks frontpage variable in counts.
 *
 * @return int number of crons to run
 */
function revlinks_get_total_cron_runs() {
  // If no more pages to parse, send back zero
  if (empty(revlinks_get_page_count())) {
    return 0;
  }
  // Use overrides if passed, instead of site vars (if calling from drush)
  $perBatch     = variable_get('revlinks_batch_amount', 0);
  $batchPerCron = variable_get('revlinks_batches_per_cron', 0);
  $perSite      = variable_get('revlinks_batch_max', 0);
  if (!empty($batchPerCron) && $batchPerCron == 'all') {
    // For all batches in one cron
    return 1;
  }
  // Calculate over multiple crons
  if (!empty($perBatch) && !empty($batchPerCron) && (!empty($perSite) || $perSite == 'all')) {
    $pagesToParseCount = revlinks_get_page_count();
    // If real total < max total on form, use real total
    if ($perSite == 'all' || ($pagesToParseCount < $perSite)) {
      $perSite = $pagesToParseCount;
    }
    // Add frontpage to count if checked - not needed since homepage is run as extra batch on each cron now.
    //$perSite = !empty(variable_get('revlinks_parse_frontpage', 0)) ? $perSite+1 : $perSite;
    $numBatches = $perSite / $perBatch;
    $numCrons = ceil($numBatches / $batchPerCron);
    return $numCrons;
  }
  else {
    return 0;
  }
}

/**
 * Calculations
 */

/**
 * Delete entities to prep for any re-parsing
 *
 * This is mainly for update mode, but if default is spread
 * over multiple crons, and a previous page is edited/published,
 * it will get re-parsed
 *
 * @todo change name from 'check' to something describing deletions
 *
 * @param $results array of objects with node table query results
 */
function revlinks_reparse_check($results) {
  if (!empty($results)) {
    $nids = [];
    foreach ($results as $result) {
      $nids[] = $result->nid;
    }
    _revlinks_sql_delete_records($nids);
  }
}

/**
 * Get count of pages available to parse in data source table.
 *
 * @return mixed
 *   FALSE if no types were picked yet, or a count
 */
function revlinks_get_page_count() {
  // Only proceed if at least one node type is selected to parse
  if (!empty(revlinks_get_parse_types())) {
    if (revlinks_get_mode() == 'default') {
      // Also check count of pages updated between last run and now
      // that are less than last_parsed_nid.
      $betweenPages = _revlinks_sql_get_node_pages_between_runs();
      // @todo make sure count results works here
      $betweenPagesCount = count($betweenPages);
      $pageCount = _revlinks_sql_get_page_count();
      $results = $pageCount + $betweenPagesCount;
    }
    else {
      // Update mode can count revlinks table
      $results = _revlinks_sql_get_page_count();
    }
    return $results;
  }
  else {
    return FALSE;
  }
}

/**
 * Get total count of links parsed.
 *
 * @return int
 */
function revlinks_get_links_parsed_count() {
  return _revlinks_sql_parsed_page_count();
}

/**
 * Get last parsed page nid.
 *
 * @return int
 */
function revlinks_get_last_parsed_nid() {
  return variable_get('revlinks_last_parsed_nid', 0);
}

/**
 * Loop through page result objects to parse links for entities
 *
 * Translation nodes will have different nids
 * and get picked up to parse.
 *
 * @param array $results
 *   Array of db query results from data source table
 * @param int $from counter
 *   Start point
 * @param int $count
 *   Amount per run
 *
 * @return array
 *    Multi assoc arrays of key/value pairs for a set of entities
 */
function revlinks_format_entities_array($results, $from, $count) {
  $mode = revlinks_get_mode();
  $baseUrl = rtrim(variable_get('revlinks_base_url', ''), '/');
  $entities = [];
  $date = new DateTime();
  $timestamp = $date->getTimestamp();
  // Loop counters
  $start = $from;
  $end = $from + $count;
  for ($i = $start; $i < $end; $i++) {
    if (isset($results[$i])) {
      $result = $results[$i];
      // Check for homepage
      if ($result->nid == 0) {
        $parsedUrl = variable_get('site_frontpage', '');
      }
      else {
        // Send full url to parse, but save relative url to db
        // default mode: need to build lang url
        // update mode: already have lang in path var
        if ($mode == 'default') {
          $parsedUrl = url('node/' . $result->nid, ['language' => $result->language]);
        }
        else {
          $parsedUrl = $result->path; // lang already in path field
        }
      }
      $links = revlinks_get_links_on_page($baseUrl . '/' .$parsedUrl);
      foreach ($links as $link) {
        $entities[] = [
          'revlink_nid' => '',
          'parsed_nid' => isset($result->nid) ? $result->nid : '',
          'parsed_path' => $parsedUrl,
          'parsed_title' => $result->title,
          'parsed_type' => $result->type,
          'link_title' => $link['title'],
          'link_url' => trim($link['url'], '/'),
          'created' => $timestamp
        ];
      }
    }
  }
  return $entities;
}

/**
 * Pass url to scraper for links
 *
 * @param string $url
 *   Absolute url to page
 *
 * @return mixed
 *   Array holding a LinkScraper object or FALSE if error
 */
function revlinks_get_links_on_page($url = '') {
  try {
    $scraper = new LinkScraper($url);
    $links = $scraper->getInternalLinksOnPage(revlinks_get_options());
    return $links;
  } catch (Exception $e) {
    watchdog('Revlinks', $e->getMessage(), [], WATCHDOG_ERROR);
    return FALSE;
  }
}

/**
 * Translate totalCount from variable or 'all' to actual number or 0 for none.
 *
 * @param mixed $totalCount
 *   Int or 'all' string
 *
 * @return int of total count, 0 is for none
 */
function revlinks_check_total_count($totalCount = 0) {
  // If set to 'all' run query from lookup method
  // elseif we've parsed all pages, return zero.
  if ($totalCount == 'all') {
    $totalCount = revlinks_get_page_count();
  }
  elseif (variable_get('revlinks_total_pages_parsed') >= $totalCount) {
    $totalCount = 0;
  }
  return $totalCount;
}


/**
 * Create/update/delete data
 */

/**
 * Save new entities to Drupal
 *
 * @param $entityValues
 *
 * @return bool
 *   True/False on success
 */
function revlinks_create_entities($entityValues) {
  foreach ($entityValues as $reverseLink) {
    $entityType = 'revlink';
    $entity = entity_create($entityType, $reverseLink);
    $wrapper = entity_metadata_wrapper($entityType, $entity);
    $wrapper->save();
  }
  return TRUE;
}

/**
 * Delete all records in revlinks table
 *
 * @return object TruncateQuery
 */
function revlinks_delete_entities() {
  $result = _revlinks_sql_delete_revlinks_entities();
  watchdog('Revlinks', 'Emptied revlinks table of entities.', [], WATCHDOG_INFO);
  return $result;
}

/**
 * Delete all homepage records in revlinks table
 *
 * @return int
 */
function revlinks_delete_homepage_entities() {
  $result = _revlinks_sql_delete_homepage_records();
  watchdog('Revlinks', 'Deleted homepage links in revlinks table, most likely to re-parse page.', [], WATCHDOG_INFO);
  return $result;
}

/**
 * Delete vars stored from admin settings form
 */
function revlinks_delete_variables() {
  $vars = [
    'revlinks_base_url',
    'revlinks_skip_classes',
    'revlinks_batch_amount',
    'revlinks_batch_max',
    'revlinks_test_url',
    'revlinks_node_types',
    'revlinks_node_types_to_parse',
    'revlinks_main_class',
    'revlinks_parse_frontpage',
    'revlinks_parse_anchors',
    'revlinks_update_mode',
    'revlinks_first_run_date',
    'revlinks_last_run_date',
    'revlinks_last_parsed_nid',
    'revlinks_debug_mode',
    'revlinks_total_pages_parsed',
    'revlinks_batches_per_cron',
  ];
  foreach ($vars as $var) {
    variable_del($var);
  }
  watchdog('Revlinks', 'Deleted module variables.', [], WATCHDOG_INFO);
}
