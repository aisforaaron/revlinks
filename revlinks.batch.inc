<?php

/**
 * @file Batch methods to prepare data and process
 */

/**
 * Run batch from Elysia Cron module
 *
 * Called from revlinks_cronapi()
 */
function revlinks_batch_start_elysia() {
  $mode = revlinks_get_mode();
  $batchCount    = variable_get('revlinks_batch_amount', 10);
  $totalBatches  = variable_get('revlinks_batches_per_cron', '');
  $totalCount    = revlinks_check_total_count(variable_get('revlinks_batch_max', ''));
  if (!empty($totalCount) || ($mode == 'update' && !empty(variable_get('revlinks_parse_frontpage', 0)))) {
    _revdebug('Send to batch_prep with perBatch/totalPages/perCron ' . $batchCount . '/' . $totalCount . '/' . $totalBatches);
    $batch = revlinks_batch_prep($batchCount, $totalCount, $totalBatches);
    batch_set($batch);
    $batch =& batch_get();
    $batch['progressive'] = FALSE;
    batch_process('admin/config/system/cron');
  }
  else {
    watchdog('Revlinks', 'No more pages left to parse with current settings.', [], WATCHDOG_INFO);
  }
}

/**
 * Run batch from drush command
 *
 * $ drush revlinks
 *
 * See README.md for more details
 */
function revlinks_batch_start_drush() {
  $mode = revlinks_get_mode();
  $batchCount    = variable_get('revlinks_batch_amount', 10);
  $totalBatches  = variable_get('revlinks_batches_per_cron', '');
  $totalCount    = revlinks_check_total_count(variable_get('revlinks_batch_max', ''));
  // If we have records to parse OR we are in update mode and need to parse homepage only
  if (!empty($totalCount) || ($mode == 'update' && !empty(variable_get('revlinks_parse_frontpage', 0)))) {
    _revdebug('Send to batch_prep with perBatch/totalPages/perCron ' . $batchCount . '/' . $totalCount . '/' . $totalBatches);
    $batch = revlinks_batch_prep($batchCount, $totalCount, $totalBatches);
    batch_set($batch);
    $batch =& batch_get();
    $batch['progressive'] = FALSE;
    drush_backend_batch_process();
  }
  else {
    watchdog('Revlinks', 'No more pages left to parse with current settings.', [], WATCHDOG_INFO);
  }
}

/**
 * Setup batch variables
 *
 * @param int $batchCount
 *   amount of pages per batch to parse
 * @param int $totalCount
 *   amount of total pages to parse
 * @param mixed $totalBatches
 *   amount of total batches per cron run or 'all'
 *
 * @return array
 *   Batch API multi array with pages to parse
 */
function revlinks_batch_prep($batchCount = 0, $totalCount = 0, $totalBatches = 0) {
  $mode = revlinks_get_mode();
  $totalPagesParsed = ($mode == 'update') ? 0 : variable_get('revlinks_total_pages_parsed', 0);
  $batch = [
    'title' => t('Revlinks parsing'),
    'finished' => 'revlinks_batch_finished',
    'file' => drupal_get_path('module', 'revlinks') .'/revlinks.module',
    'init_message' => t('Revlinks Batch is starting'),
    'progress_message' => t('Parsing batch @current out of @total'),
    'error_message' => t('Revlinks Batch has encountered an error'),
  ];
  // Figure out how many pages we can parse for this cron
  if ($totalBatches != 'all') {
    // 10*2=20 for 2 batches of 10
    $pageAmount = $batchCount * $totalBatches;
    // If totalCount is 14 (less than 20) only need 14, so use totalCount
    $pageAmount = ($totalCount < $pageAmount) ? $totalCount : $pageAmount;
  }
  else {
    // If we are getting all pages for one cron
    $pageAmount = $totalCount;
  }
  $pages = revlinks_get_pages($totalPagesParsed, $pageAmount, FALSE);
  _revdebug('get_pages with totalParsed/pageAmount: ' . $totalPagesParsed . '/' . $pageAmount .' -- ' . print_r($pages, TRUE));

  // Get any nodes that have been published
  // in between cron runs that might be skipped.
  // This depends on where last parsed nid is located.
  // Add them to the pages table to be parsed in update mode.
  // This is only needed in 'default' mode, since 'update' mode will catch any changes to new/old nodes.
  $updatedPages = ($mode == 'default') ? _revlinks_sql_get_node_pages_between_runs() : [];
  if (!empty($updatedPages)) {
    _revdebug('get_pages_between: ' . print_r($updatedPages, TRUE));
    // See if we should add or update pages table w/node info
    foreach ($updatedPages as $updatedPage) {
      $type = $updatedPage->type;
      $nid = !empty($updatedPage->nid) ? $updatedPage->nid : 0;
      // @todo add language prefix to path here?
      $path = url('node/' . $nid, ['language' => $updatedPage->language]);
      $title = $updatedPage->title;
      $changed = $updatedPage->changed;
      // @todo make sure $id returned properly
      // Node could have been created before but not added to pages table.
      $id = _revlinks_sql_check_page($nid);
      $updateType  = (!empty($id)) ? 'Updated' : 'Added';
      // Returns true/false if record insert/update worked
      $result = _revlinks_sql_save_page([
        'id'      => $id,
        'path'    => $path,
        'nid'     => $nid,
        'title'   => $title,
        'type'    => $type,
        'changed' => $changed
      ]);
      $status = ($result) ? 'Success' : 'Error';
      watchdog('Revlinks', ':status on :updateType page :page to parse while in default mode, for run in update mode.',
        [':status' => $status, ':updateType' => $updateType, ':page' => $path],WATCHDOG_INFO);
    }
  }

  // If we are only parsing homepage, skip the rest
  if (!empty($pages)) {
    // Clear out records from revlinks that will be re-parsed
    // this includes the frontpage if it's not already removed and enabled for parsing
    revlinks_reparse_check($pages);
    // How many batches to run?
    // - all: based on total pages per batch, calculate amount of batches needed
    // - fixed number: if less than needed amount, some may not get parsed
    // if 'all' turn to max number for stats
    if ($totalBatches == 'all') {
      $totalBatches = ceil($totalCount / $batchCount);
    }
    // Loop through each batch and create import operations.
    // Split the main pages array into smaller batch arrays.
    // This will use set amount of batches per cron,
    // or put all batches for one cron based on above if statement.
    for($i = 0; $i < $totalBatches; $i++) {
      $start = $batchCount * $i;
      $pageBatch = array_slice($pages, $start, $batchCount);
      _revdebug('pageBatch ' . print_r($pageBatch, TRUE));
      if (!empty($pageBatch)) {
        $batch['operations'][] = ['_revlinks_parse_page', [$pageBatch]];
      }
    }
  }
  // Add last batch for frontpage if enabled
  if (variable_get('revlinks_parse_frontpage', 0)) {
    // Delete homepage links for re-parse if this isn't first run
    if (!empty(variable_get('revlinks_first_run_date', 0))) {
      // @todo add try/catch here, if error - don't reparse homepage?
      revlinks_delete_homepage_entities();
    }
    $frontPage = new stdClass();
    $frontPage->nid = 0;
    $frontPage->title = 'homepage';
    $frontPage->type = 'frontpage';
    $homePage = [$frontPage];
    $batch['operations'][] = ['_revlinks_parse_page', [$homePage]];
  }
  return $batch;
}

/**
 * Batch operation to parse one batch of pages
 *
 * @param array $pages
 *   Array of page objects to parse
 * @param array $context
 *   Reference var to pass vars between batch runs
 */
function _revlinks_parse_page($pages = [], &$context) {
  // Record batch start times
  $context['results']['batch_times'][] = time();
  // Parse pages
  $entityValues = revlinks_format_entities_array($pages, 0, count($pages));
  // Save links found to entities
  revlinks_create_entities($entityValues);
  // Get highest nid in batch
  $lastNid = isset($context['results']['last_parsed_nid']) ?
    $context['results']['last_parsed_nid'] : variable_get('revlinks_last_parsed_nid', 0);
  foreach ($pages as $page) {
    $lastNid = ($page->nid > $lastNid) ? $page->nid : $lastNid;
  }
  // After creating link records of parsed pages,
  // clear out those pages from update table to not re-parse again.
  if (revlinks_get_mode() == 'update') {
    $nids = [];
    $newPages = [];
    foreach ($pages as $page) {
      // Don't save homepage nid 0 here, it's not in pages table to remove
      if($page->nid != 0) {
        $nids[] = $page->nid;
        $newPages[] = $page->new; // bool int
      }
    }
    if (!empty($nids)) {
      _revlinks_sql_delete_pages($nids);
      watchdog('Revlinks',
        'Deleted records from revlinks_pages (not links) table that were just parsed. Nids :nids ',
        [':nids' => implode(', ', $nids)],
        WATCHDOG_INFO
      );
    }
    // Update mode counter, only add new pages to counter
    // new nodes are set to 1 in an array, just get sum of array
    $context['results']['total_pages_parsed'] += array_sum($newPages);
  }
  else {
    // Default mode counter, all pages are new during batch runs
    // Don't count frontpage
    if ($pages[0]->nid != 0) {
      _revdebug('Updating pages counter by +' . count($pages));
      $context['results']['total_pages_parsed'] += count($pages);
    }
  }
  $context['results']['total_links_added'] += count($entityValues);
  if (!empty($lastNid)) {
    $context['results']['last_parsed_nid'] = $lastNid;
  }
}

/**
 * Batch finished callback method.
 *
 * @param $success
 * @param array $results
 * @param $operations
 */
function revlinks_batch_finished($success, $results, $operations) {
  // Figure out batch process time length
  if (isset($results['batch_times'])) {
    $nowTime = time();
    $processTime = _revlinks_batch_time($results['batch_times'][0], $nowTime);
    // Save run timestamps
    if (empty(variable_get('revlinks_first_run_date', 0))) {
      variable_set('revlinks_first_run_date', $nowTime);
    }
    variable_set('revlinks_last_run_date', $nowTime);
    // For both default/update mode
    $updatedCount = variable_get('revlinks_total_pages_parsed', 0) + $results['total_pages_parsed'];
    variable_set('revlinks_total_pages_parsed', $updatedCount);
  }
  else {
    $processTime = '(no time counted)';
    $updatedCount = '?'; // if not set, need to debug
  }
  if ($success) {
    // Keep previous nid if in update mode and only parsing frontpage
    $lastNid = !empty($results['last_parsed_nid']) ? $results['last_parsed_nid'] : variable_get('revlinks_last_parsed_nid', 0);
    variable_set('revlinks_last_parsed_nid', $lastNid);
    $message = t(
      '@count links successfully parsed in @time. Current total pages parsed is @total. Highest nid parsed @last_nid.',
      [
        '@count' => $results['total_links_added'],
        '@time' => $processTime,
        '@total' => $updatedCount,
        '@last_nid' => $lastNid,
      ]
    );
    watchdog('Revlinks', $message, [], WATCHDOG_INFO);
  }
  else {
    $error_operation = reset($operations);
    $message = t('An error occurred while processing %error_operation with arguments: @arguments', [
      '%error_operation' => $error_operation[0],
      '@arguments'       => print_r($error_operation[1], TRUE),
    ]);
    watchdog('Revlinks', $message, [], WATCHDOG_ERROR);
  }
}

/**
 * Helper method to calculate time of batch run.
 *
 * @param string $start unix timestamp
 * @param string $nowTime unix timestamp
 *
 * @return string of hour:min:sec
 */
function _revlinks_batch_time($start = '', $nowTime = '') {
  $dtStart = new DateTime();
  $dtStart->setTimestamp($start);
  $dtEnd = new DateTime();
  $dtEnd->setTimestamp($nowTime);
  $interval = $dtEnd->diff($dtStart);
  return $interval->h . ':' . $interval->i . ':' . $interval->s;
}
