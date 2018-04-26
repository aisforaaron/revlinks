## Revlinks Module


### Goal

To store data on internal links used on node pages, and display this data on node screens for users with permission.

Recommended Strategy
- Run all pages and all batches first, then switch to update mode and wait for cron runs to parse new/updated pages.


### Overview

This module will start off in 'default' mode and query the node table to get a list of node ids. Each cron run will look at this list and decide what to parse based on amount per batch and last parsed node. 

If 'parse homepage' is enabled in the config form, the homepage will get parsed as a single item in an extra batch added during each cron run. For 'default' mode, once it has parsed all pages, subsequent crons will not trigger any more parsing of homepage. For 'update' mode, the homepage will be parsed each time.

A node is parsed from code by calling file_get_contents on the full cached url, then running the html code through an HTML Purifier library to find internal links. These links are stored in the Drupal site as a custom entity called a "Revlink" in the revlinks table. No nodes are updated or created during any point of this module. Just the entities.

When 'update' mode is enabled, the module will check to see if any new or updated node is saved with a published state, and add a record to a custom table called revlinks_pages. When cron runs in 'update' mode, this module will only parse nodes saved to this table with the exception of the homepage.

If a page that has already been parsed is updated and published, the module will delete those "Revlink" entities and set the node to re-parse. 

This module is completely self contained and can be enabled/disabled/uninstalled without affecting any other node or feature content. If you uninstall the module, the custom tables and variables stored in the Drupal variable table will be deleted.


### Dependencies

* See module listing page or .info file for Module dependencies
  * Elysia Cron module is supported but not needed for Revlinks to run
  * Redirect module data will be checked if enabled, but not a dependency
* Composer manager helps with parser class vendor libraries found in composer.json


### Installation

1. Make sure you meet other module dependencies, as noted above.
1. Enable the module "Revlinks" which will manage parsing and storing the revlink entities.
1. Set admin/view permissions for any roles under "Revlinks" on the Drupal permissions page.
1. Go to the admin config page for this module to configure settings. (/admin/config/search/reverse-links)
1. Run the batch process:
  * Use drush to run it. "drush nrl --help" will show you more examples.
  * Or force run the cron job revlinks_batch_start on the /admin/config/system/cron page, or setup a cron to run this on a schedule.
1. Wait for report to run, then check one of two places of a content type you picked from the admin config page:
   * A node to see the "Revlinks" tab
   * On the edit screen at the bottom, in the vertical tabs, there will be a fieldset with reverse link information


### Usage

* A custom Drush command is available 
  * Module defaults (second one uses command alias 'nrl' without extra messaging)
     * <code>$ drush reverse-links --verbose</code>
     * <code>$ drush nrl</code>

* Elysia Cron
  * a forced run from Cron settings page (/admin/config/system/cron) with job name __revlinks_batch_start_elysia: Batch process Revlinks__
  * scheduling the job through Elysia Cron on a schedule


### Notes

Database Tables

* Table: revlinks
  * Description: The base table for Revlink entities.
  * Fields
     * revlink_id, serial, Primary Key: Identifier for a Revlink.
     * parsed_nid, int, Node id of parsed page.
     * parsed_path, text, URL of parsed page.
     * parsed_title, text, Title of parsed page.
     * parsed_type, text, Content type of parsed node.
     * link_title, text, Parsed link title.
     * link_url, text, Parsed link url.
     * created, int, The Unix timestamp when the Revlink was created.
* Table: revlinks_pages
   * Description: Pages to parse while in update mode for Revlinks module.
   * Fields
     * id, serial, Primary Key: Identifier for a page to parse.
     * path, text, Relative URL of page to parse.
     * nid, int, Node id of page to parse if exists. Zero if no nid.
     * title, text, Title of page to parse.
     * type, text, Content type of page to parse.
     * new, int, Track if page is new, otherwise means updated and parsed before.
     * changed, int, The Unix timestamp when the page was created/updated and added here on save.

Watchdog

* Drupal watchdog is setup to log errors and batch completion messages which include runtime from first batch until finished. 

Testing

* There is a debug mode which can be enabled from the admin settings form. This will display some additional debug information in drupal_set_messages on reporting pages.

Developers

* See install file for database schema.
* All database queries are located in revlinks.sql.inc and use a naming convention <code>_revlinks_sql\_*</code>
* Some php methods have warning/notice suppression using '@' in front to avoid flooding watchdog and php error logs.
* Any JS code will need cdata wrappers to properly get purified/parsed:
```
<script type="x/templates" id="compare-modal">
 //<![CDATA[
   js stuff here...
  //]]>
</script>
```
