<?php
namespace Drupal\revlinks;

use HTMLPurifier;
use HTMLPurifier_Config;

class LinkScraper {

  public $dom = FALSE;
  public $url = '';

  function __construct($url = '') {
    $this->dom = new \PHPHtmlParser\Dom();
    $this->url = $url;
    if ($url) {
      try {
        // Suppress warning with @ if can't open file so next page will parse
        // @todo add revlinks_errors table to capture bad page urls?
        @$rawHTML = file_get_contents(str_replace('&amp;', '&', $url));
        if ($rawHTML) {
          $cleanerConfig = HTMLPurifier_Config::createDefault();
          // @todo make this configurable through settings page?
          // Move the cache directory somewhere else (no trailing slash):
          // create dir if doesn't exist
          $cachePath = variable_get('file_public_path') . '/Serializer/Revlinks';
          if (!file_exists($cachePath)) {
            mkdir($cachePath, 0700, true);
          }
          $cleanerConfig->set('Cache.SerializerPath', $cachePath);
          $cleaner = new HTMLPurifier($cleanerConfig);
          $formattedHTML = $cleaner->purify($rawHTML);
          $this->dom->loadStr($formattedHTML, []);
        }
        else {
          $this->dom = FALSE;
        }
      }
      catch (SomeException $e) {
        $message = 'LinkScraper error on page ' . $url . '<br>Error Message: ' . $e->getMessage();
        watchdog('Revlinks', $message, [], WATCHDOG_ERROR);
      }
    }
  }

  /**
   * Scraper object called to get html from url
   *
   * @param array $options vars to decide to save link or not
   *
   * @return array $links
   *   Array with keys: url, title
   */
  public function getInternalLinksOnPage($options = []) {
    $output = [];
    try {
      // Make sure we can access the page before running find()
      if ($this->dom) {
        $contents = $this->dom->find($options['mainClass']);
        foreach ($contents as $content) {
          $links = $this->dom->loadStr($content, [])->getElementsbyTag('a');
          foreach ($links as $link) {
            $linkArr = [
              'url'   => $link->getAttribute('href'),
              'title' => $link->innerHTML,
              'classes' => explode(' ', $link->getAttribute('class')),
            ];
            if ($this->validateLinkToSave($linkArr, $options) == TRUE){
              $output[] = $linkArr;
            }
          }
        }
      }
    }
    catch (SomeException $e) {
      $message = 'LinkScraper error in getInternalLinksOnPage ' . '<br>Error Message: ' . $e->getMessage();
      watchdog('Revlinks', $message, [], WATCHDOG_ERROR);
    }
    return $output;
  }

  /**
   * Figure out if we want to save a link
   *
   * @param array $linkArr
   *   Info on link being parsed
   * @param array $options
   *   Vars to decide to save link or not
   *
   * @return bool
   */
  public function validateLinkToSave($linkArr, $options) {
    if (!empty($options['skipClasses']) && !empty($linkArr['classes']) && array_intersect($options['skipClasses'], $linkArr['classes'])) {
      // Skip based on class
      return false;
    }
    elseif ($options['skipAnchors'] && substr($linkArr['url'], 0, 1) == '#') {
      // Skip based on local anchor
      return false;
    }
    elseif (url_is_external($linkArr['url'])) {
      // Skip external links
      return false;
    }
    else {
      return true;
    }
  }

}
