<?php

/**
 * @file Databse queries for module
 *
 * Internal module lookup methods for all SQL queries.
 */

/**
 * Get pages from node table
 *
 * Conditions:
 *   content types from allowed list in module settings
 *   node status is published
 *   order results so highest nid is parsed last
 *   if range is passed, only get a set of the total results
 *
 * @param array $range assoc array of from/count integers
 *
 * @return array indexed of stdClass objects
 */
function _revlinks_sql_get_node_pages($range = []) {
  $result = db_select('node', 'n')
    ->fields('n', ['nid', 'type', 'title', 'language'])
    ->condition('n.type', revlinks_get_parse_types(), 'IN')
    ->condition('n.status', 1)
    ->orderBy('n.nid', 'ASC');
  if (!empty($range)) {
    $result->range($range['from'], $range['count']);
  }
  return $result->execute()->fetchAll();
}

/**
 * Get nodes updated in between cron runs
 *
 * Use case for this method
 * - User updated unpub node #3 but last_parsed_nid=10.
 *   Next batch starts at nid 11, but need to parse nid 3.
 * - Cron #2 starts
 * - get all pub pages and start w/nid > 11
 * - get all pub pages changed between last_run_date and NOW
 *   that are less than last_parsed_nid
 *   since greater than nids will get picked up already
 *
 * @return array indexed of stdClass objects
 */
function _revlinks_sql_get_node_pages_between_runs() {
  // Check if we are past first cron
  // If we have a lastRunDate, use that
  // otherwise it's the first time running and we should return empty object
  // @todo return empty object and skip query if first run?
  $lastRunDate = variable_get('revlinks_last_run_date', 0);
  $changed = !empty($lastRunDate) ? $lastRunDate : time();
  $lastParsedNid = variable_get('revlinks_last_parsed_nid', 0);
  $query = db_select('node', 'n')
    ->fields('n', ['nid', 'type', 'title', 'language', 'changed'])
    ->condition('type', revlinks_get_parse_types(), 'IN')
    ->condition('status', 1)
    ->condition('changed', [$changed, time()], 'BETWEEN')
    ->condition('nid', $lastParsedNid, '<')
    ->orderBy('nid', 'ASC')
    ->execute()
    ->fetchAll();
  return $query;
}

/**
 * Get pages to parse from revlinks_pages table for update mode only
 *
 * @return array indexed of stdClass objects
 */
function _revlinks_sql_get_updated_node_pages() {
  return db_select('revlinks_pages', 'rp')
    ->fields('rp')
    ->orderBy('id')
    ->execute()
    ->fetchAll();
}

/**
 * Get page count of all available pages to parse from source table
 *
 * This doesn't include front page.
 * Force return to an int since db_select returns string.
 *
 * Conditions:
 *   type (content types) from allowed list in module settings
 *   if mode is 'update' only get new/updated nodes after last run
 *
 * @return int number of results
 */
function _revlinks_sql_get_page_count() {
  if (revlinks_get_mode() == 'update') {
    $result = db_select('revlinks_pages', 'rp')
      // @todo page count work for just one field? do i need fields at all?
      ->fields('rp', ['id'])
      ->countQuery()
      ->execute()
      ->fetchField();
  }
  else {
    $result = db_select('node', 'n')
      ->fields('n', ['nid'])
      ->condition('type', revlinks_get_parse_types(), 'IN')
      ->condition('n.status', 1)
      ->condition('n.nid', revlinks_get_last_parsed_nid(), '>')
      ->countQuery()
      ->execute()
      ->fetchField();
  }
  return (int) $result;
}

/**
 * Get count of pages already parsed from revlinks table
 *
 * Force return to an int since db_select returns string.
 *
 * @return int number of pages
 */
function _revlinks_sql_parsed_page_count() {
  return (int) db_select('revlinks', 'r')
    ->fields('r', ['revlink_id'])
    ->countQuery()
    ->execute()
    ->fetchField();
}

/**
 * Get aliases for a node from url_alias table
 *
 * @param string $path
 *   Path of node to check
 *
 *  // @todo ???@return mixed array of assoc keyed array or NULL
 *
 * @return mixed array indexed of stdClass objects or NULL
 */
function _revlinks_sql_get_aliases($path = '') {
  if (!empty($path)) {
    return db_select('url_alias', 'ua')
      ->fields('ua', ['alias'])
      ->condition('source', $path)
      ->execute()
      ->fetchAll();
      //->fetchAllKeyed(0, 0);
  }
  else {
    return NULL;
  }
}

/**
 * Get redirects for a node from redirect table
 *
 * @param string $path
 *   Path of node to check
 *
 * @return mixed array of assoc keyed array or NULL
 */
function _revlinks_sql_get_redirects($path = '') {
  if (!empty($path)) {
    return db_select('redirect', 'r')
      ->fields('r', ['source'])
      ->condition('r.redirect', $path)
      ->execute()
      ->fetchAllKeyed(0, 0);
  }
  else {
    return NULL;
  }
}

/**
 * Check path in pages table
 *
 * Should only get one record per nid.
 * Force return to an int since db_select returns string.
 *
 * @param int $nid
 *   Node id if exists for page to parse
 *
 * @return int bool or NULL
 */
function _revlinks_sql_check_page($nid = 0) {
  if (!empty($nid)) {
    return (int) db_select('revlinks_pages', 'rp')
      ->fields('rp', ['id'])
      ->condition('nid', $nid)
      ->execute()
      ->fetchField();
  }
  else {
    return NULL;
  }
}

/**
 * Get all links from revlinks table that point to a node
 *
 * Build in sql args if they exist
 *
 * @param string $path to node we are checking
 * @param string $aliases of node
 * @param string $redirects of node
 *
 * Conditions:
 *   Any variation of the url that points to node/# is returned
 *
 * @return array of objects
 */
function _revlinks_sql_get_revlinks($path = '', $aliases = '', $redirects = '') {
  $result = db_select('revlinks', 'r')
    ->fields('r')
    ->condition('r.link_url', $path);
  // Setup OR condition checks
  $or = db_or();
  $orFlag = FALSE;
  // Add aliases
  if (!empty($aliases)) {
    $or->condition('r.link_url', $aliases,'IN' );
    $orFlag = TRUE;
  }
  // Add redirects
  if (!empty($redirects)) {
    $or->condition('r.link_url', $redirects,'IN' );
    $orFlag = TRUE;
  }
  // Add OR condition
  if ($orFlag) {
    $result->condition($or);
  }
  return $result->execute()->fetchAll();
}

/**
 * Get a random sample of records from reverse links table
 *
 * @return array indexed of stdClass objects
 */
function _revlinks_sql_get_revlinks_sample() {
  $results = db_select('revlinks', 'r')
    ->fields('r')
    ->condition('r.parsed_nid', '0', '<>')
    ->range(0, 10)
    ->orderRandom()
    ->execute()
    ->fetchAll();
  return $results;
}

/**
 * Delete all links from revlinks table parsed from homepage
 *
 * Conditions:
 *   parsed links for homepage have parsed_nid of 0
 *
 * Returns number of records deleted.
 *
 * @return int of records deleted
 */
function _revlinks_sql_delete_homepage_records() {
  return db_delete('revlinks')
    ->condition('parsed_nid', 0)
    ->execute();
}

/**
 * Delete links from revlinks table by parsed_nid
 *
 * @param array nid
 *   Node ids to search for in table
 *
 * Returns number of records deleted.
 *
 * @return mixed int of records deleted or NULL
 */
function _revlinks_sql_delete_records($nids = []) {
  if (!empty($nids)) {
    return db_delete('revlinks')
      ->condition('parsed_nid', $nids, 'IN')
      ->execute();
  }
  else {
    return NULL;
  }
}

/**
 * Insert or update a record in pages table
 *
 * If $fields['id'] exists, then it's an update.
 *
 * @param array $fields
 *   Assoc array of fields and values to add to query
 *   Allowed fields: 'path', 'nid', 'title', 'type', 'changed'
 *
 * @return mixed bool FALSE or strings SAVED_NEW|SAVED_UPDATED
 */
function _revlinks_sql_save_page($fields = []) {
  if (!empty($fields)) {
    $primaryKeys = (isset($fields['id']) && !empty($fields['id'])) ? $fields['id'] : NULL;
    $allowed = ['path', 'nid', 'title', 'type', 'changed'];
    $queryFields = [];
    // fields['id'] is skipped here
    foreach ($fields as $key => $value) {
      if (in_array($key, $allowed) && !empty($value)) {
        $queryFields[$key] = $value;
      }
    }
    // Returns FALSE if failure
    return drupal_write_record('revlinks_pages', $queryFields, $primaryKeys);
  }
  else {
    return FALSE;
  }
}

/**
 * Delete records in pages table
 *
 * @param array $nids
 *   Array of node ids
 * @param bool $all
 *   Flag to decide to truncate pages table
 *
 * @return mixed object of db_truncate|db_delete or NULL
 */
function _revlinks_sql_delete_pages($nids = [], $all = FALSE) {
  if ($all == TRUE) {
    return db_truncate('revlinks_pages')
      ->execute();
  }
  elseif (!empty($nids)) {
    return db_delete('revlinks_pages')
      ->condition('nid', $nids)
      ->execute();
  }
  else {
    return NULL;
  }
}

/**
 * Delete records in revlinks table
 *
 * @return object db_truncate
 */
function _revlinks_sql_delete_revlinks_entities() {
  return db_truncate('revlinks')
    ->execute();
}
