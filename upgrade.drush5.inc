<?php

/**
 * @file
 *   Refine your Drupal major version upgrade.
 */

/**
 * Implement hook_drush_command().
 */
function upgrade_drush_command() {
  $items = array();

  $items['site-upgrade'] = array(
    'description' => 'Run a major version upgrade for Drupal (e.g. Drupal 6 to Drupal 7).  A copy of the site is made, and then upgraded; the source site is not changed.',
    'drupal dependencies' => array('update'),
    'drush dependencies' => array('sql', 'pm', 'core'),
    'core' => array(6), // Add 7 once drush supports 7 -> 8 upgrades.
    'arguments' => array(
      'target' => "The name of a sitealias, which points to the destination site. 'root' and 'uri' keys are required; db-url is recommended. See examples/aliases.drushrc.php for more information about creating a site alias.",
      'stage' => "Name (or partial name) of stage to start at",
    ),
    'examples' => array(
      'drush site-upgrade @onward' => 'Upgrade from the current site to the site specified by @onward alias.',
    ),
    'options' => array(
      'confirm-all' => 'Usually, Drush site-upgrade will run all of the straightforward steps automatically, and only prompts at "important" steps.  --confirm-all forces prompts at every step.',
      'skip-optional' => 'Never prompt for optional steps; just skip them. This is the default; --skip-optional is only necessary with --confirm-all.',
      'auto' => 'Automatically select the most reasonable choice at each prompt, when one is available.',
      'structure-tables-key' => 'A key in the structure-tables array. @see example.drushrc.php. Defaults to \'common\'.',
      'source-dump' => 'Path to dump file. Medium or large sized sites should set this. Optional; default is to create a temporary file.',
      'db-su' => 'DB username to use when dropping and creating the target database. Optional.',
      'db-su-pw' => 'DB password to use when dropping and creating the target database. Optional.',
      'cache' => 'Use pm-download cache to speed up re-download from drupal.org.',
      'no-cache' => 'Transfer a fresh database from source site. Otherwise, DB dump is re-used for 24 hours. Negates --cache as well.',
      'core-only' => 'Stop after upgrading Drupal core; do not download and enable new versions of the site\'s modules.',
      'force-sites-default' => 'Forces settings.php to be written in sites/default folder, even if source settings.php is not.',
      'replace' => 'Replace target if it already exists.  Default is to prompt.',
      'reuse' => 'Reuse target if it already exists.  Default is to prompt.',
      'uninstall' => 'Comma-separated list of modules to uninstall in the target database prior to upgrade.  n.b. The source site is not affected.',
    ),
    'aliases' => array('sup'),
    'topics' => array('docs-aliases'),
  );
  $items['site-upgrade-prepare'] = array(
    'description' => 'Prior to running updatedb on Drupal core, disable all contrib modules and uninstall any module the user specified should be uninstalled.  Called automatically by site-upgrade.',
    'hidden' => TRUE,
    'arguments' => array(
      'modules' => 'The modules to disable.',
    ),
    'options' => array(
      'uninstall' => 'Comma-separated list of modules to uninstall prior to upgrade.',
    ),
  );
  $items['site-upgrade-progress'] = array(
    'description' => 'Show information about the upgrade status of the target site.',
    'bootstrap' => 'DRUSH_BOOTSTRAP_DRUPAL_ROOT',
    'aliases' => array('upgrade-progress', 'supp'),
  );
/*
  $items['site-upgrade-modules'] = array(
    'description' => 'Download, enable, and run updatedb on all contrib modules after an upgrade.  Called automatically by site-upgrade.',
    'hidden' => TRUE,
    'arguments' => array(
      'modules' => 'The modules to download and enable.',
    ),
    'options' => array(
      'projects' => 'The projects that must be downloaded to provide the specified module list.',
    ),
  );
*/
  $items['docs-upgrading'] = array(
    'description' => 'Upgrading Drupal using the drush site-upgrade command.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'callback' => 'drush_print_file',
    'callback arguments' => array(dirname(__FILE__) . '/upgrade.html'),
  );
  return $items;
}

/**
 * _drush_upgrade_site_upgrade_stages
 *
 * This function returns the stages of the finite state machine
 * that controls the major upgrade process.  The key of each item
 * in the stages array is the stage name; the first stage is always
 * START, and the last stage is always STOP.
 *
 * The value of each item in the stages array is the control object.
 * The elements of the control object are as follows:
 *
 *  - description:
 *
 *    Shown to the user before the stage begins.  When running with --prompt,
 *    the user will be asked whether to perform the step, skip it (do it manually)
 *    or to stop.  Perhaps this should be the default, and --no-prompt should be 
 *    required to skip prompts.  Note that other confirmations will come up under 
 *    --no-prompt; as usual, these may be skiped with --yes.
 *
 *  - omit-stage-header:
 *
 *    Leave off the header with the stage name that is usually displayed
 *    above the stage description.
 *
 *  - no-op:
 *
 *    There is no operation associated with this stage; for example, it
 *    might just be a stage that prompts the user before going on to the
 *    next step.  Without the 'no-op' flag, Drush will advise the user
 *    that they must do the step manually if there is no 'callback' item.
 *
 *  - verify:
 *
 *    The name of a function callback that Drush will call to verify whether
 *    or not the current step, perhaps, may already have been performed.  If
 *    the verify function returns TRUE, then the current step will be skipped
 *    with a $stage_result of TRUE, without prompting the user.  Otherwise, the 
 *    stage will run as usual.
 *
 *  - verify-message:
 *
 *    The verify message will be printed if the verify function returns TRUE.
 *
 *  - callback:
 *
 *    The name of the function callback that Drush will call to perform the
 *    step.  If not specified, Drush will say: "Drush cannot perform this step;
 *    you must do it manually."
 *
 *  - backup:
 *
 *    If set, a backup of the target site will be saved if the callback completes
 *    successfully. The backup will be named per the value of this item.
 *
 *  - next:
 *
 *    In its simplest form, 'next' is a string that describes the next 
 *    stage to proceed to.  The following are equivalent:
 *
 *      'next' => 'name of next stage',
 *
 *            IS THE SAME AS
 *
 *      'next' => array(
 *        'ok' => 'name of next stage',
 *        'error' => 'STOP',
 *      ),
 *
 *    The return value from the callback function is used to select the
 *    next stage of the FSM to advance to.  TRUE is mapped to 'ok',
 *    FALSE is mapped to 'error'.  Other return values are possible,
 *    provided that they align with an entry in the 'next' array.
 *
 *  - selection-options:
 *
 *    Provides the list of prompts to present to the user.
 *
 *      '_run' => 'Do it for me.'
 *      '#skip' => 'Skip this step.'
 *      '#manual' => 'I will do this step manually.'
 *
 *    n.b. Key values that start with # will cause the callback function
 *    to be skipped with a $stage_result of TRUE, whereas all other
 *    values will call the callback function.  If the callback function
 *    is skipped and the selected option exists in the 'next' array,
 *    then the $stage_result will be set to the selected option key.
 *
 *  - required:
 *
 *    If specified, then the option to skip the step or do it manually is
 *    not provided.  This is used for stages that gather required information
 *    that will be used later, for example.  The value of this variable
 *    is used to replace the prompt for the '_run' item in the selection options.
 *
 *  - optional:
 *
 *    If specified, the user will be advised that this step is optional
 *    (does not always need to be done).
 *
 *  - not-always-needed:
 *
 *    Behaves like 'optional', but is not skipped by --skip-optional.
 *
 *  - unnecessary:
 *
 *    If specified, the user will be advised that this step is not necessary
 *    (does not apply when using the Drush workflow).
 *    Unnecessary steps are automatically considered optional.
 *
 *  - straightforward:
 *
 *    This step does not take much thought or consideration on the part
 *    of the user.  Straightforward steps are by default run automatically
 *    (without prompting).  They will be prompted if --confirm-all is
 *    selected.
 *
 */
function _drush_upgrade_site_upgrade_stages() {
  $stages['START'] = array(
    'omit-stage-header' => TRUE,
    'continue' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Step 1',
  );
  // This list of stages is taken from UPGRADE.txt from Drupal 7.10.
  // The intention is for the stages to track changes in UPGRADE.txt,
  // and for the code to track the descriptions in the stages table.
  // Report discrepancies in the drush_sup issue queue.
  $stages['UPGRADE.txt Major Upgrade Step 1'] = array(
    'description' =>
      "
        1. Check on the Drupal 7 status of your contributed and custom modules and
           themes. See http://drupal.org/node/948216 for information on upgrading
           contributed modules and themes. See http://drupal.org/node/895314 for a list
           of modules that have been moved into core for Drupal 7, and instructions on
           how to update them. See http://drupal.org/update/modules for information on
           how to update your custom modules, and http://drupal.org/update/theme for
           custom themes.

           You may decide at this point that you cannot upgrade your site, because
           needed modules or themes are not ready for Drupal 7.
      ",
    'callback' => 'drush_upgrade_check_modules_and_themes',
    'continue' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Prompt',
  );
  $stages['UPGRADE.txt Major Upgrade Prompt'] = array(
    'prompt' => 'Please review the upgread readiness report. You may want to uninstall modules that are not ready to upgrade yet.',
    'omit-stage-header' => TRUE,
    'no-op' => TRUE,
    'selection-options' => array('#skip' => 'Begin upgrade'),
    'next' => 'UPGRADE.txt Major Upgrade Step 2',
  );
  $stages['UPGRADE.txt Major Upgrade Step 2'] = array(
    // TODO: Eventually we will be picking our finite state machine stages
    // based on the version of Drush being upgraded, so we'll be able to
    // rewrite the text below to presume a 6.x-to-7.x upgrade.
    'description' =>
      "
        2. Update to the latest available version of Drupal 6.x (if your current version
           is Drupal 5.x, you have to upgrade to 6.x first). If you need to update,
           download Drupal 6.x and follow the instructions in its UPGRADE.txt. This
           document only applies for upgrades from 6.x to 7.x.
      ",
    'note' => 'You must also insure that all of your contrib modules are up to date!',
    'verify' => '_drush_upgrade_site_already_latest_version_drupal',
    'selection-options' => array('#skip' => 'Continue upgrade with out-of-date Drupal (Not recommended -- failure probable).'),
    'next' => 'UPGRADE.txt Major Upgrade Step 3',
  );
  $stages['UPGRADE.txt Major Upgrade Step 3'] = array(
    'description' =>
      "
        3. Log in as user ID 1 (the site maintenance user).
      ",
    'continue' => TRUE,
    'unnecessary' => 'You will not need to log in if Drush is used to perform all upgrade steps for you.',
    'next' => 'UPGRADE.txt Major Upgrade Step 4',
  );
  $stages['UPGRADE.txt Major Upgrade Step 4'] = array(
    'description' =>
      "
        4. Go to Administer > Site configuration > Site maintenance. Select
           \"Off-line\" and save the configuration.
      ",
    'verify' => '_drush_upgrade_site_already_offline',
    'verify-message' => 'The source site is already offline.',
    'drush-command' => 'variable-set',
    'drush-arguments' => array('maintenance_mode', '1'),
    'drush-options' => array('always-set' => TRUE),
    'optional' => 'When Drush does the steps of your site upgrade, it always modifies the working copy of your site; the original site is left unchanged. You may wish to take your site off-line anyway.',
    'next' => 'UPGRADE.txt Major Upgrade Step 5',
  );
  $stages['UPGRADE.txt Major Upgrade Step 5'] = array(
    'description' =>
      "
        5. Go to Administer > Site building > Themes. Enable \"Garland\" and select it as
           the default theme.
      ",
    'optional' => 'Drush will set the administrative theme to "Garland" in the target site without affecting the source site.',
    'next' => 'UPGRADE.txt Major Upgrade Step 6',
  );
  $stages['UPGRADE.txt Major Upgrade Step 6'] = array(
    'description' =>
      "
        6. Go to Administer > Site building > Modules. Disable all modules that are not
           listed under \"Core - required\" or \"Core - optional\". It is possible that some
           modules cannot be disabled, because others depend on them. Repeat this step
           until all non-core modules are disabled.

           If you know that you will not re-enable some modules for Drupal 7.x and you
           no longer need their data, then you can uninstall them under the Uninstall
           tab after disabling them.
      ",
    'unnecessary' => 'Drush will automatically disable all non-core modules in the target site without affecting the source site. You may disable or uninstall contrib modules now if you wish; any that you do not do now will be disabled by Drush after Step 12.',
    'next' => 'UPGRADE.txt Major Upgrade Step 7',
  );
  $stages['UPGRADE.txt Major Upgrade Step 7'] = array(
    'description' =>
      "
        7. On the command line or in your FTP client, remove the file

             sites/default/default.settings.php
      ",
    'unnecessary' => 'Drush will remove default.settings.php from the target site later, after Step 12.  You may remove it from the source sit now if you wish, but doing so is unnecessary.',
    'next' => 'UPGRADE.txt Major Upgrade Step 8',
  );
  $stages['UPGRADE.txt Major Upgrade Step 8'] = array(
    'description' =>
      "
        8. Remove all old core files and directories, except for the 'sites' directory
           and any custom files you added elsewhere.

           If you made modifications to files like .htaccess or robots.txt, you will
           need to re-apply them from your backup, after the new files are in place.
      ",
    'unnecessary' => 'Drush will create your upgraded site in a new location; it is not necessary to remove any core files from the source site.',
    'next' => 'UPGRADE.txt Major Upgrade Step 9',
  );
  $stages['UPGRADE.txt Major Upgrade Step 9'] = array(
    'description' =>
      "
        9. If you uninstalled any modules, remove them from the sites/all/modules and
           other sites/*/modules directories. Leave other modules in place, even though
           they are incompatible with Drupal 7.x.
      ",
    'unnecessary' => 'Drush will create your upgraded site in a new location; it is not necessary to remove any contrib module files from the source site.',
    'next' => 'UPGRADE.txt Major Upgrade Step 10',
  );
  $stages['UPGRADE.txt Major Upgrade Step 10'] = array(
    'description' =>
      "
        10. Download the latest Drupal 7.x release from http://drupal.org to a
            directory outside of your web root. Extract the archive and copy the files
            into your Drupal directory.

            On a typical Unix/Linux command line, use the following commands to download
            and extract:

              wget http://drupal.org/files/projects/drupal-x.y.tar.gz
              tar -zxvf drupal-x.y.tar.gz

            This creates a new directory drupal-x.y/ containing all Drupal files and
            directories. Copy the files into your Drupal installation directory:

              cp -R drupal-x.y/* drupal-x.y/.htaccess /path/to/your/installation

            If you do not have command line access to your server, download the archive
            from http://drupal.org using your web browser, extract it, and then use an
            FTP client to upload the files to your web root.
      ",
    // TODO: If running in "reuse" mode, notify the user that Drupal is already in place, and skip this step. As it is, this step will seem to run, but will do nothing.
    'callback' => '_drush_upgrade_site_download_next_version_drupal',
    'straightforward' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Step 11',
  );
  $stages['UPGRADE.txt Major Upgrade Step 11'] = array(
    'description' =>
      "
        11. Re-apply any modifications to files such as .htaccess or robots.txt.
      ",
    'not-always-needed' => "If you had no modifications to .htaccess or robots.txt, you don't need to do anything here.",
    'next' => 'UPGRADE.txt Major Upgrade Step 12',
  );
  $stages['UPGRADE.txt Major Upgrade Step 12'] = array(
    'description' =>
      "
        12. Make your settings.php file writeable, so that the update process can
            convert it to the format of Drupal 7.x. settings.php is usually located in

             sites/default/settings.php
      ",
    'note' => 'Note that Drush will instead *copy* your settings.php file to its new location.  The copy is made writable, but the original may remain write-protected.',
    'callback' => '_drush_upgrade_site_make_settings_writable',
    'straightforward' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Step 12-a',
  );
  $stages['UPGRADE.txt Major Upgrade Step 12-a'] = array(
    'description' =>
      "
        12-a. Drush-specific step:  Drush will now do steps 5 and 6 of UPGRADE.txt, 
              and set the site theme to Garland and disable all contrib modules.  
              Before it does this, it makes a copy of your database, and does
              all modifications on the *copy*, leaving your source site unchanged.
              Drush will also uninstall all modules specified via the --uninstall option
              at this time.
      ",
    'callback' => '_drush_upgrade_site_copy_database_and_disable_contrib',
    'straightforward' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Step 12-b',
  );
  $stages['UPGRADE.txt Major Upgrade Step 12-b'] = array(
    'description' =>
      "
        12-b. Drush-specific step:  Drush will now do step 7 of UPGRADE.txt, and
              remove sites/default/default.settings.php from the target site.
      ",
    'verify' => '_drush_upgrade_site_verify_default_settings_removed',
    'callback' => '_drush_upgrade_site_remove_default_settings',
    'straightforward' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Step 13',
  );
  $stages['UPGRADE.txt Major Upgrade Step 13'] = array(
    'description' =>
      "
        13. Run update.php by visiting http://www.example.com/update.php (replace
            www.example.com with your domain name). This will update the core database
            tables.

            If you are unable to access update.php do the following:

            - Open settings.php with a text editor.

            - Find the line that says:
              \$update_free_access = FALSE;

            - Change it into:
              \$update_free_access = TRUE;

            - Once the upgrade is done, \$update_free_access must be reverted to FALSE.
      ",
    'straightforward' => TRUE,
    'note' => 'Note that Drush will copy your database and run updatedb on the copy.  The source database remains unchanged.',
    'drush-command' => 'updatedb',
    'drush-site' => '@upgrade-target',
    'drush-backend-options' => array('interactive' => TRUE),
    'next' => 'UPGRADE.txt Major Upgrade Step 14',
  );
  $stages['UPGRADE.txt Major Upgrade Step 14'] = array(
    'description' =>
      "
        14. Backup your database after the core upgrade has run.
      ",
    'backup' => 'upgraded-core',
    'backup_message' => "Upgraded Core",
    'straightforward' => TRUE,
    'callback' => '_drush_upgrade_site_backup_after_updatedb',
    'next' => 'UPGRADE.txt Major Upgrade Step 15',
  );
  $stages['UPGRADE.txt Major Upgrade Step 15'] = array(
    'description' =>
      "
        15. Replace and update your non-core modules and themes, following the
            procedures at http://drupal.org/node/948216
      ",
    'note' => 'Drush will walk through these instructions for each module.  Note that all of the new modules will be downloaded up-front now; they will be moved into place later.',
    'prepare' => '_drush_site_upgrade_reset_module_to_upgrade',
    'verify' => '_drush_site_upgrade_select_contrib_module_to_upgrade',
    'callback' => '_drush_site_upgrade_prepare_module_upgrade',
    'next' => array(
      'ok' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 1',
      '#done' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Finish Up',
    ),
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 1'] = array(
    'description' =>
      "
         1. Check your notes, and the UPGRADE.txt files inside each module, to see if any special upgrade instructions apply
      ",
    'prepare' => '_drush_site_upgrade_show_module_upgrade_txt',
    'no-op' => TRUE,
    'required' => TRUE,
    'next' => array(
      'ok' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 2',
      '#postpone' => 'UPGRADE.txt Major Upgrade Step 15',
    ),
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 2'] = array(
    'description' =>
      "
         2. Fully remove the old module directory inside sites/all/modules/
      ",
    'unnecessary' => 'Note that Drush builds the target site in a new directory; the modules directory therefore starts off empty, and there is therefore no need to delete anything. If there is for some reason an old module directory in place, Drush will automatically delete it when downloading the new module.',
    'next' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 3',
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 3'] = array(
    'description' =>
      "
         3. Download, unpack and move the new module directory to sites/all/modules/
      ",
    'straightforward' => TRUE,
    'prepare' => '_drush_site_upgrade_show_module_location',
    'verify' => '_drush_site_upgrade_verify_module_code_exists',
    'verify-message' => 'The correct module already exists at the destination directory.',
    'callback' => '_drush_site_upgrade_get_module_code',
    'next' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 4',
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 4'] = array(
    'description' =>
      "
         4. Enable the module
      ",
    'straightforward' => TRUE,
    'callback' => '_drush_site_upgrade_enable_module',
    'next' => array(
      'ok' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 5',
      'error' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Handle Enable Errors',
    ),
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 5'] = array(
    'description' =>
      "
         5. Run update.php again (you should have run it once already when upgrading core) and check for any message output.
      ",
    'straightforward' => TRUE,
    // TODO: It would be nifty if we could include a verify function that determined if the module needed any database updates.
    // This should probably be done up-front in step 15.
    'drush-command' => 'updatedb',
    'drush-site' => '@upgrade-target',
    'drush-options' => array('yes' => TRUE),
    'drush-backend-options' => array('interactive' => TRUE),
    'next' => array(
      'ok' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 6',
      'error' => 'Upgrading Contrib Modules from Drupal 6 to Drupal 7 Handle Update Errors',
    ),
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Step 6'] = array(
    'description' =>
      "
         6. Back up your database and files again, between each module update, so that if there is a problem, you can revert to the most recent update.
      ",
    'straightforward' => TRUE,
    'backup' => 'upgraded-contrib',
    'backup_message' => "Upgraded Contrib Module !module_to_upgrade",
    'next' => 'UPGRADE.txt Major Upgrade Step 15',
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Finish Up'] = array(
    'description' =>
      "
         If you have a Drupal 7 version of the theme you were using for Drupal 6, place it in sites/all/themes/ and reenable it.
      ",
    'next' => 'Post-upgrade Step A',
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Handle Enable Errors'] = array(
    'description' =>
      "
        If you have errors when you attempt to enable a module:

        Note any error messages you see.
        Check to see if perhaps Drush simply could not find the module; this can happen if a module
        is moved to another project.  If this happens, you can manually download the new project and
        Drush will automatically recover.
      ",
    'not-always-needed' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Step 15',
  );
  $stages['Upgrading Contrib Modules from Drupal 6 to Drupal 7 Handle Update Errors'] = array(
    'description' =>
      "
        If you have errors when you run the update.php script:

        Note any error messages you see.
        Restore your site to its previous state, using the file and database backups you created before you started the upgrade or update process. Do not attempt to do further updates or upgrades on a site that had update problems.
        Consult one of the support options listed on http://drupal.org/support.
      ",
    'next' => 'UPGRADE.txt Major Upgrade Step 15',
  );
  $stages['Post-upgrade Step A'] = array(
    'description' =>
      "
        A. Copy files directory from the source site to the upgrade site.
      ",
    'callback' => '_drush_site_upgrade_copy_files',
    'next' => 'Post-upgrade Step B',
  );
  $stages['Post-upgrade Step B'] = array(
    'description' =>
      "
        B. Enable the core module content_migrate.
      ",
    'callback' => '_drush_site_upgrade_enable_content_migrate',
    'next' => 'Post-upgrade Step C',
  );
  $stages['Post-upgrade Step C'] = array(
    'description' =>
      "
        C. Fix up some file system permissions.
      ",
    'callback' => '_drush_site_upgrade_fix_permissions',
    'next' => 'Post-upgrade Step D',
  );
  $stages['Post-upgrade Step D'] = array(
    'description' =>
      "
        D. Run cron, for good measure.
      ",
    'callback' => '_drush_site_upgrade_run_cron',
    'next' => 'UPGRADE.txt Major Upgrade Step 16',
  );
  $stages['UPGRADE.txt Major Upgrade Step 16'] = array(
    'description' =>
      "
        16. Go to Administration > Reports > Status report. Verify that everything is
            working as expected.
      ",
    'note' => 'Drush can also show you this information in the console via the `drush status-report` command.',
    'drush-command' => 'status-report',
    'drush-site' => '@upgrade-target',
    'next' => 'UPGRADE.txt Major Upgrade Step 17',
  );
  $stages['UPGRADE.txt Major Upgrade Step 17'] = array(
    'description' =>
      "
        17. Ensure that \$update_free_access is FALSE in settings.php.
      ",
    'straightforward' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Step 18',
  );
  $stages['UPGRADE.txt Major Upgrade Step 18'] = array(
    'description' =>
      "
        18. Go to Administration > Configuration > Development > Maintenance mode.
           Disable the \"Put site into maintenance mode\" checkbox and save the
           configuration.
      ",
    'straightforward' => TRUE,
    'next' => 'STOP',
  );
  return $stages;
}

/**
 * Implement hook_drush_help().
 */
function upgrade_drush_help($section) {
  switch ($section) {
    case 'drush:site-upgrade':
      return dt("Execute a major version upgrade for Drupal core and enabled contrib modules. Command will download next version of Drupal and all available contrib modules that have releases. It prepares a settings.php for the target site, and copies the prior version's database to the target site. Finally, updatedb is run. The intent is for developers to keep re-running this command until they are satisfied with the resulting site. Run this command from within your source site (D6). Note that this command uses pm-download and sql-sync internally so most options for those commands are valid here too.");
  }
}

/**
 * Command argument complete callback.
 *
 * @return
 *  Array of available site aliases.
 */
function upgrade_site_upgrade_complete() {
  return array('values' => array_keys(_drush_sitealias_all_list()));
}

/**
 * Give a report on the progress of an upgrade
 */
function drush_upgrade_site_upgrade_progress($fsm_context = NULL) {
  if (!is_array($fsm_context)) {
    $fsm_context = _drush_upgrade_load_fsm_context(drush_get_context('DRUSH_SELECTED_DRUPAL_ROOT'));
  }
  if (array_key_exists('stage', $fsm_context)) {
    drush_print("The current stage is " . $fsm_context['stage']);
    drush_print("The complete list of contrib extensions is: " . implode(',', $fsm_context['contrib_to_upgrade']));
    drush_print("The following contrib extesnions have no available releases in the target version: " . implode(',', $fsm_context['unavailable_extensions']));
    if (array_key_exists('module_to_upgrade', $fsm_context)) {
      drush_print("The next module to upgrade is: " . $fsm_context['module_to_upgrade']);
    }
    if (!empty($fsm_context['extensions_with_problems'])) {
      drush_print("The following contrib extesnions had problems: ");
      foreach ($fsm_context['extensions_with_problems'] as $extension => $info) {
        $message = $info['message'];
        drush_print("   $extension: $message");
      }
    }
    drush_print_r($fsm_context['target_alias']);
    drush_print("The project cache is at " . $fsm_context['project_cache']);
    foreach ($fsm_context as $key => $value) {
      if (!is_array($value)) {
        if ($value === TRUE) {
          $value = "true";
        }
        if ($value === FALSE) {
          $value = "false";
        }
        drush_print("$key: $value");
      }
    }
  }
  else {
    return drush_set_error('DRUSH_NO_UPGRADE_IN_PROGRESS', dt("There is no site upgrade in progress on the target site."));
  }
}

/**
 * Do some sanity checks to make sure that we are ready to perform an upgrade, and
 * that the command is being called with reasonable-looking parameters.
 */
function drush_upgrade_site_upgrade_check_parameters($target_key = NULL) {
  if (empty($target_key)) {
    drush_print();
    return drush_set_error('DRUSH_UPGRADE_NO_TARGET', dt('No target argument specified.  To perform the site upgrade, define an alias record for the target site.'));
  }

  if (!$target_alias = drush_sitealias_get_record($target_key)) {
    return drush_set_error('DRUSH_UPGRADE_NO_TARGET', dt('Site alias not found: @target-key. See example.drushrc.php.', array('@target-key' => $target_key)));
  }

  if (!file_exists(dirname($target_alias['root']))) {
    return drush_set_error('DRUSH_UPGRADE_NO_TARGET', dt('Parent directory for site alias root not found: @root; this folder must exist before running site-upgrade. See example.drushrc.php.', array('@root' => dirname($target_alias['root']))));
  }

  if (realpath($target_alias['root']) == realpath(DRUPAL_ROOT)) {
    return drush_set_error('DRUSH_UPGRADE_NO_TARGET', dt('Target site alias must have a different Drupal root directory than the source site.  Both are at @root.', array('@root' => $target_alias['root'])));
  }

  return TRUE;
}

function _drush_upgrade_load_fsm_context($destination_core) {
  $fsm_context = array();
  $cache_file = $destination_core. '/upgrade-context.serialized';
  if (file_exists($cache_file)) {
    $fsm_context = (array)unserialize(file_get_contents($cache_file));
  }
  $fsm_context['fsm_context_cache_file'] = $cache_file;
  return $fsm_context;
}

/**
 * Main command hook for site-upgrade.
 *
 * This runs bootstrapped to the SOURCE site.
 */
function drush_upgrade_site_upgrade($target_key = NULL, $initial_stage = NULL) {
  $stage = '';
  // Set 'invoke' so that the commands we invoke do not complain about unknown options.
  drush_set_option('invoke', TRUE);

  // TODO: Look up appropriate upgrade stages for the version of Drupal being upgraded.
  $stages = _drush_upgrade_site_upgrade_stages();
 
  // Partial feature: start at some stage in the middle.
  // Warning: does not validate whether site is in any condition
  // to resume at this stage or not. Use at your own risk!
  // This will probably be removed in favor of enhancing the
  // above fsm context restore code to allow the user to select
  // any backup as a point to resume from.
  if ($initial_stage) {
    $pattern = preg_replace('/ +/', '.*', $initial_stage);
    $pattern = preg_replace('/([0-9]+[\.a-z0-9]*)/', '$1\b', $initial_stage);
    $pattern = "/.*(" . $pattern . ").*/";
    $alternatives = array();
    foreach ($stages as $name => $control) {
      if (preg_match($pattern, $name, $matches)) {
        $alternatives[$name] = $name;
      }
    }
    if (empty($alternatives)) {
      return drush_set_error('DRUSH_SITE_UPGRADE_STAGE_NOT_FOUND', dt("Could not find !stage", array('!stage' => $initial_stage)));
    }
    elseif (count($alternatives) == 1) {
      $stage = array_shift($alternatives);
    }
    else {
      $stage = drush_choice($alternatives, dt("Which step would you like to start on?"));
      if (!$stage) {
        return drush_user_abort();
      }
    }
  }

  // PREPARE:  Find the target version and determine the contrib projects and enabled modules installed.

  $fsm_context['source_version'] = drush_drupal_major_version();
  $fsm_context['target_version'] = $fsm_context['source_version'] + 1;
  $fsm_context['fsm_context_cache_file'] = FALSE;
  $fsm_context['uninstall_extensions'] = drush_get_option('uninstall', '');
  $fsm_context['core-only'] = drush_get_option('core-only', FALSE);
  $source_record = drush_sitealias_get_record('@self');
  $fsm_context['source_name'] = (!empty($source_record) ? $source_record['#name'] . '-' : '') . 'd' . $fsm_context['source_version'];
  if (isset($target_key)) {
    $fsm_context['target_key'] = $target_key;
    $fsm_context['target_alias'] = drush_sitealias_get_record($target_key);
    if (!empty($fsm_context['target_alias'])) {
      $fsm_context['destination_core'] = $fsm_context['target_alias']['root'];
      $fsm_context['target_name'] = (!empty($fsm_context['target_alias']) ? $fsm_context['target_alias']['#name'] . '-' : '') . 'd' . $fsm_context['target_version'];
      $fsm_context['work_name'] = $fsm_context['source_name'] . '-to-' . $fsm_context['target_name'];
      $fsm_context['work_dir'] = drush_directory_cache($fsm_context['work_name']);
      $fsm_context['project_cache'] = $fsm_context['work_dir'] . '/project_cache';
      $fsm_context['backup_dir'] = $fsm_context['work_dir'] . '/progress_backups';
      drush_mkdir($fsm_context['project_cache']);
      drush_mkdir($fsm_context['backup_dir']);

      $fsm_context['destination_conf_path'] = conf_path();
      if (drush_get_option('force-sites-default')) {
        $fsm_context['destination_conf_path'] = 'sites/default';
      }

      // Load the previous fsm context, if it exists.
      $previous_fsm_context = _drush_upgrade_load_fsm_context($fsm_context['destination_core']);
      $fsm_context['fsm_context_cache_file'] = $previous_fsm_context['fsm_context_cache_file'];
      // If the user explicitly specified a stage to start at, then force
      // a 'resume'.
      if (!empty($stage)) {
        $fsm_context = $previous_fsm_context;
      }
      // If a site already exists at the destination, ask the user what to do.
      elseif (is_dir($fsm_context['destination_core'])) {
        // TODO:  validate that $previous_fsm_context is compatible with $fsm_context
        // and the upgrade has not completed.
        if (array_key_exists('stage', $previous_fsm_context)) {
          $options['resume'] = dt("Resume the upgrade at the stage it left off at: !stage", array('!stage' => $previous_fsm_context['stage']));
        }
        if (is_dir($fsm_context['destination_core'])) {
          $options['reuse'] = dt("Re-use the existing code: re-import the database from the source site and update core and contrib modules again.");
        }
        $options['replace'] = dt("Delete the existing target: start over from the beginning.");
        $backup_files = drush_scan_directory($fsm_context['backup_dir'], '/.*\.serialized$/', array('.', '..', 'CVS'));
        $backups = array();
        foreach ($backup_files as $backup_file => $info) {
          $backup_name = basename($backup_file, '.serialized');
          $backup_archive = dirname($backup_file) . '/' . $backup_name . '.tar';
          if (file_exists($backup_archive)) {
            $backup_context = (array)unserialize(file_get_contents($backup_file));
            $backups[$backup_name] = array(
              'archive' => $backup_archive,
              'context' => $backup_context
            );
            $options[$backup_name] = dt("Restore backup '!message' and resume upgrade", array('!message' => $backup_context['backup_message']));
          }
        }
        
        $selection = drush_choice($options, dt("A Drupal site exists at the destination. What would you like to do?"));
        // Selected a backup item?
        if (array_key_exists($selection, $backups)) {
          $archive = $backups[$selection]['archive'];
          $fsm_context = $backups[$selection]['context'];
          $stage = $fsm_context['stage'];
          drush_upgrade_site_upgrade_progress($fsm_context);
          drush_delete_dir($fsm_context['destination_core'], TRUE);
          $values = drush_invoke_process('@none', 'archive-restore', array($archive), array('destination' => $fsm_context['destination_core'], 'overwrite' => TRUE));
        }
        else {
          switch ($selection) {
            case 'replace':
              drush_delete_dir($fsm_context['destination_core'], TRUE);
              break;
            case 'resume':
              $fsm_context = $previous_fsm_context;
              $stage = $fsm_context['stage'];
              break;
            case 'reuse':
              // Move every project from the destination site's sites/all/modules and sites/all/themes directory
              // into the project cache.  TODO:  Maybe we should put the saved files somewhere else.
              foreach (array('sites/all/modules', 'sites/all/themes') as $dir) {
                $scan_dir = $fsm_context['destination_core'] . '/' . $dir;
                $projects = drush_scan_directory($scan_dir, '/./', array('.', '..', 'CVS'), 0, 0);
                foreach ($projects as $project_path => $info) {
                  $target_dir = $fsm_context['project_cache'] . '/' . basename($project_path);
                  if (is_dir($target_dir)) {
                    drush_delete_dir($target_dir, TRUE);
                  }
                  rename($project_path, $target_dir);
                }
              }
              break;
            default:
              return drush_user_abort();
          }
        }
      }
    }
  }
  // These flags always take on the value reflected by the cli options, regardless
  // of what might have been cached in the previous fsm context file.
  $fsm_context['auto'] = drush_get_option('auto', FALSE);
  $fsm_context['straightforward'] = (!drush_get_option('confirm-all')) | $fsm_context['auto'];
  $fsm_context['skip-optional'] = drush_get_option('skip-optional', $fsm_context['straightforward']);

  // Erase the 'download_attempts' variable on each run
  $fsm_context['download_attempts'] = array();

  $fsm_context += array(
    // TODO: we could probably determine the version of the database by inspectiion (e.g. has node_update_7000 run?)
    // However, we should probably also keep track of what version we think the target database file -should- be,
    // even if we use inspection on the current db, so we know if the active db is in sync with the stage of the
    // upgrade process we think we are in.
    'target_database_version' => 0, // No database exists -- until we copy it. Then we will set this to source_version. After updatedb, we set it to target_version.
    'extensions_with_problems' => array(),
  );

  // Collect information about the system before we begin.
  drush_upgrade_gather_information($fsm_context);
  if (empty($stage)) {
    $stage = 'START';
  }
  // Start up the FSM and run the upgrade
  _drush_upgrade_site_state_machine($fsm_context, $stages, $stage);
}

function _drush_upgrade_rewrap_msg($msg) {
  $msg = preg_replace("/^[ \t]*/m", "", $msg);
  $msg = preg_replace("/\n\n+/s", "<br>", $msg);
  $msg = preg_replace("/[ \t\n]+/s", " ", $msg);
  $msg = preg_replace("/<br>/s", "\n\n", $msg);
  $msg = wordwrap($msg, drush_get_context('DRUSH_COLUMNS', 80) - 5);
  if (preg_match("/^ *([0-9-]+\. +)/", $msg, $matches)) {
    $msg = preg_replace("/^/m", str_pad('', strlen($matches[1])), $msg);
  }
  return trim($msg);
}

/**
 * Makes a replacements array for use with dt();
 * includes a !key'ed replacement for every item
 * in the fsm context that has a string value.
 */
function _drush_site_upgrade_replacements($fsm_context) {
  $replacements = array();
  foreach ($fsm_context as $key => $value) {
    if (is_string($value)) {
      $replacements['!' . $key] = $value;
    }
  }
  return $replacements;
}

function _drush_upgrade_site_state_machine(&$fsm_context, $stages, $initial_stage) {
  $stage = $initial_stage;
  $fsm_context += array(
    'auto' => FALSE,
    'straightforward' => FALSE,
    'skip-optional' => FALSE,
  );
  while ($stage != 'STOP') {
    $fsm_context['stage'] = $stage;
    $control = $stages[$stage];
    $control += array(
      'omit-stage-header' => FALSE,
      'no-op' => FALSE,
      'description' => '',
      'required' => '',
      'optional' => '',
      'not-always-needed' => '',
      'unnecessary' => '',
      'note' => '',
      'straightforward' => FALSE,
      'continue' => FALSE,
      'callback' => NULL,
      'backup' => FALSE,
      'drush-command' => FALSE,
      'verify' => NULL,
      'prepare' => NULL,
      'verify-message' => FALSE,
      'prompt' => 'What would you like to do?',
      'selection-options' => array(),
      'next' => 'STOP',
    );
    // TODO: call an 'alter' callback on the control structure (YNGNI?)
    if ($control['prepare']) {
      $control['prepare']($fsm_context, $control);
    }
    if (!is_array($control['next'])) {
      $control['next'] = array('ok' => $control['next']);
    }
    $control['next'] += array('error' => 'STOP');
    if (!$control['omit-stage-header']) {
      drush_print("\n" . str_pad("", drush_get_context('DRUSH_COLUMNS', 80) - 2, ':'). "\n" . $stage);
    }
    if (array_key_exists('module_to_upgrade', $fsm_context)) {
      drush_print(dt("Upgrading module: !module_to_upgrade", _drush_site_upgrade_replacements($fsm_context)));
    }
    foreach (array('description', 'optional', 'not-always-needed', 'unnecessary', 'note') as $message_key) {
      if (!empty($control[$message_key])) {
        drush_print("\n" . _drush_upgrade_rewrap_msg($control[$message_key]));
      }
    }
    $control['callback'] = $control['callback'];
    $control['selection'] = '_run';
    $stage_result = TRUE;
    $prompt_user = TRUE;
    // If the 'verify' function returns TRUE, that indicates that the
    // step has already been completed.  In that instance, the stage
    // is skipped without prompting the user.
    if (($control['verify']) && ($control['verify']($fsm_context, $control) === TRUE)) {
      $control['selection'] = '#verified';
      $prompt_user = FALSE;
      $stage_result = TRUE;
      if ($control['verify-message']) {
        drush_print("\n" . $control['verify-message']);
      }
    }
    elseif ($control['continue']) {
      $control['selection'] = 'continue';
      $prompt_user = FALSE;
      $stage_result = TRUE;
    }
    if ($control['drush-command']) {
      $control['callback'] = '_drush_upgrade_site_drush_command';
    }
    $has_selection_options = !empty($control['selection-options']);
    if (($control['callback'] || $control['backup']) && $control['straightforward'] && $fsm_context['straightforward']) {
      $prompt_user = FALSE;
      drush_print();
      drush_print(dt("Drush will automatically do this step now."));
    }
    if (($control['optional'] || $control['unnecessary']) && $fsm_context['skip-optional']) {
      $control['selection'] = '#skip';
      $prompt_user = FALSE;
      drush_print();
      drush_print(dt("Skipping optional step."));
    }
    if ($prompt_user) {
      $auto = $fsm_context['auto'];
      if (!$control['callback']) {
        if (empty($control['unnecessary']) && !$control['no-op']) {
          drush_print(dt("\nDrush cannot do this step; you must perform it manually."));
          $control['selection-options'] += array(
            '#manual' => dt("I will do this step manually."),
          );
          $auto = FALSE;
        }
        elseif (!$has_selection_options) {
          $control['selection-options'] += array(
            '#skip' => dt("Continue."),
          );
        }
      }
      elseif (!empty($control['required'])) {
        $control['selection-options']['_run'] = $control['required'];
      }
      elseif (!$has_selection_options) {
        $control['selection-options'] += array(
          '_run' => dt("Do it for me."),
          '#manual' => dt("I will do this step manually."),
        );
      }
      if (!empty($control['optional']) || (!empty($control['not-always-needed'])) || !empty($control['unnecessary'])) {
        $control['selection-options'] += array(
          '#skip' => dt("Skip this step."),
        );
      }

      // In auto mode, we will always select the first option for the user.
      if (!empty($control['selection-options'])) {
        if ($auto) {
          $control['selection'] = key($control['selection-options']);
          drush_print(dt("Drush will now do the step '!stage'.", array('!stage' => $control['selection-options'][$control['selection']])));
        }
        else {
          drush_print();
          $control['selection'] = drush_choice($control['selection-options'], $control['prompt']);
          if (!$control['selection']) {
            // If a stage is aborted, it is not recorded in the history.
            return drush_user_abort();
          }
        }
      }
    }
    // TODO: call a 'post selection' callback on the control structure (YNGNI?)
    // If the user's selection has the same key as one of the 'next' steps,
    // this overrides the callback item, and advances the FSM directly to 
    // the specified stage.
    if (array_key_exists($control['selection'], $control['next'])) {
      $stage_result = $control['selection'];
    }
    elseif (($control['selection']{0} != '#') && $control['callback']) {
      $stage_result = $control['callback']($fsm_context, $control);
    }
    // Convert TRUE / FALSE results into 'ok' / 'error'.
    if ($stage_result === TRUE) {
      $stage_result = 'ok';
    }
    elseif ($stage_result === FALSE) {
      $stage_result = 'error';
    }
    // If a 'required' step reported success, and if there is
    // a verify function, then we will check again to make sure
    // that it is now passing.
    if (($stage_result == 'ok') && $control['verify'] && !empty($control['required'])) {
      if ($control['verify']($fsm_context, $control) === FALSE) {
        $control['post-verify-failure'] = TRUE;
      }
    }
    // Save a history of everything done in each stage.
    $control['stage_result'] = $stage_result;
    $fsm_context['history'][] = $control;
    // Exit if there was a verify failure
    if (array_key_exists('post-verify-failure', $control)) {
      drush_print();
      return drush_set_error('DRUSH_UPGRADE_REQUIREMENT_FAILURE', dt("Verify function did not pass for a required step; aborting."));
    }
    // Check to see if we should back up the target site after this stage.
    $backup_file = FALSE;
    if (($stage_result == 'ok') && $control['backup']) {
      $backup_message = dt($control['backup_message'], _drush_site_upgrade_replacements($fsm_context));
      $backup_file = $fsm_context['backup_dir'] . '/' . $control['backup'] . '.tar';
      drush_print(dt("Making backup: '!msg' in !file", array('!msg' => $backup_message, '!file' => $backup_file)));
      $fsm_context['backups'][$control['backup']] = array(
        'message' => $backup_message,
        'file' => $backup_file,
      );
      $fsm_context['backup_message'] = $backup_message;
      $saved_context_file = $fsm_context['backup_dir'] . '/' . $control['backup'] . '.serialized';
      file_put_contents($saved_context_file, serialize($fsm_context));
    }
    // Cache the $fsm_context so that we can restart the upgrade operation again later.
    // We want to make sure this is written before the backup is made, but after the
    // information about the backup is cached in the context.  We should also cache the
    // state even in stages where a backup is not made.  We do, however, avoid saving
    // the fsm state until the destination directory exists.
    if ($fsm_context['fsm_context_cache_file'] && is_dir(dirname($fsm_context['fsm_context_cache_file']))) {
      file_put_contents($fsm_context['fsm_context_cache_file'], serialize($fsm_context));
    }
    // If a backup was requested, make it now.
    if ($backup_file) {
      $values = drush_invoke_process($fsm_context['target_key'], 'archive-dump', array(), array('destination' => $backup_file, 'overwrite' => TRUE));
    }
    // Go on to the next stage of the FSM.
    if (array_key_exists($stage_result, $control['next'])) {
      $stage = $control['next'][$stage_result];
    }
    else {
      return drush_set_error('DRUSH_UPGRADE_STATE_MACHINE_ERROR', dt("Cannot continue; no next stage."));
    }
  }
}

function _drush_upgrade_site_drush_command(&$fsm_context, $control) {
  $control += array(
    'drush-site' => '@self',
    'drush-arguments' => array(),
    'drush-options' => array(),
    'drush-backend-options' => array(),
  );
  if ($control['drush-site'] == '@upgrade-target') {
    $control['drush-site'] = $fsm_context['target_key'];
  }
  //$control['drush-options'] += array('debug' => TRUE);
  $result = drush_invoke_process($control['drush-site'], $control['drush-command'], $control['drush-arguments'], $control['drush-options'], $control['drush-backend-options']);
  return ($result['error_status'] == 0);
}

function _drush_upgrade_site_already_offline(&$fsm_context, $control) {
  $result = drush_invoke_process('@self', 'variable-get', array('maintenance_mode'));
  $site_offline = is_array($result['object']) ? array_values($result['object']) : array();
  return (count($site_offline) > 0) && ($site_offline[0] == 1);
}

function _drush_upgrade_site_already_online(&$fsm_context, $control) {
  $result = drush_invoke_process('@self', 'variable-get', array('maintenance_mode'));
  $site_offline = is_array($result['object']) ? array_values($result['object']) : array();
  return (count($site_offline) == 0) || ($site_offline[0] == 0);
}

function _drush_upgrade_site_already_latest_version_drupal(&$fsm_context, $control) {
  $result = drush_invoke_process('@self', 'pm-releases', array('drupal'));
  $installed = $result['object']['drupal']['installed'];
  $latest = array_shift(array_keys($result['object']['drupal']['releases']));
  drush_print();
  if ($installed == $latest) {
    drush_print(dt("Drupal core is up-to-date; version !installed installed.", array('!installed' => $installed)));
  }
  else {
    drush_print(dt("Drupal core is out of date; version !installed installed, latest verison is !latest.", array('!installed' => $installed, '!latest' => $latest)));
  }
  // TODO:  We should call `pm-updatecode --no` and see if it returns any out-of-date modules.
  return $installed == $latest;
}

function _drush_site_upgrade_reset_module_to_upgrade(&$fsm_context, &$control) {
  // Nothing selected yet; unset the module to upgrade from the previous iteration.
  unset($fsm_context['module_to_upgrade']);
}

function _drush_site_upgrade_select_contrib_module_to_upgrade(&$fsm_context, &$control) {
  // We will get a list of all of the enabled modules from the target site fresh every
  // time we get here, just in case something changed since last time.
  // TODO: maybe we could cache this info, and refresh it only if the user
  // stops and then resumes the site upgrade?
  $values = drush_invoke_process($fsm_context['target_key'], 'pm-list', array(), array('status' => 'enabled'), array('integrate' => FALSE));
  $target_enabled_modules = $values['object'];

  // We would like to download and enable the modules identified at
  // the beginning of the upgrade process.  However, we need to do
  // them in order, handling the ones that do not depend on any other
  // modules that have not been upgraded yet.  The issue is that
  // we can't tell what a module's dependencies are until we downloaded it.
  // Okay, let's go ahead and download everything upfront...
  $projects_to_download = array();
  $contrib_to_upgrade = array();
  $project_map = array();
  $label_map = array();
  foreach ($fsm_context['contrib_to_upgrade'] as $extension) {
    if (!array_key_exists($extension, $fsm_context['unavailable_extensions']) && !array_key_exists($extension, $contrib_to_upgrade)) {
      // There -must- be an entry for the extension in the extension_info
      // data, because $extension is the pre-upgrade name of the extension,
      // and the extension_info was built from the same source.
      if (array_key_exists($extension, $fsm_context['extension_info'])) {
        $project = $fsm_context['extension_info'][$extension]->info['project'];
      }
      else {
        $project = $extension;
        drush_log(dt("No project can be found for !extension", array('!extension' => $extension)), 'error');
      }
      // Remap the extension if its name changed
      if (array_key_exists($extension, $fsm_context['special_projects']['module-remap'])) {
        $orig_name = $extension;
        $extension = $fsm_context['special_projects']['module-remap'][$extension];
        $label = dt("!origname (now renamed to !newname)", array('!origname' => $orig_name, '!newname' => $extension));
        $label_map[$extension] = $label;
      }
      // Relocate the extension to another project if it moved
      if (array_key_exists($extension, $fsm_context['special_projects']['module-project-relocation'])) {
        $project = $fsm_context['special_projects']['module-project-relocation'][$extension];     
      }
      // Remap the project if its name changed
      elseif (array_key_exists($extension, $fsm_context['special_projects']['project-remap'])) {
        $project = $fsm_context['special_projects']['project-remap'][$project];
      }
    
      $contrib_to_upgrade[] = $extension;
      $project_map[$extension] = $project;
    }
  }
  $fsm_context['project_map'] = $project_map;
  foreach ($contrib_to_upgrade as $extension) {
    $project = $project_map[$extension];
    if (!is_dir($fsm_context['project_cache'] . '/' . $project) && ($project != 'drupal')) {
      // In 'auto' mode, don't bother to attempt to download projects more than once.
      if ((!$fsm_context['auto']) || (!in_array($project, $fsm_context['download_attempts']))) {
        $projects_to_download[$project] = $project;
        $fsm_context['download_attempts'][] = $project;
      }
    }
  }
  // If we haven't already downloaded everything on a previous iteration, grab them all now.
  // It is a little lame to keep showing the user the warning messages about all of the projects
  // that STILL are not available for download, but if the upgrade process is protracted, and
  // additional modules become available, we want to catch them at some point.
  // TODO: Maybe we should at least suppress the warning message to eliminate noise.
  if (!empty($projects_to_download)) {
    $values = drush_invoke_process('@none', 'pm-download', 
      array_keys($projects_to_download), 
      array('destination' => $fsm_context['project_cache']), 
      array('integrate' => FALSE));
  }
  // Make a list of extensions that still need to be upgraded
  $extensions_to_be_upgraded = array();
  $dependencies_map = array();
  foreach ($contrib_to_upgrade as $extension) {
    $project = $project_map[$extension];
    // Only consider extensions that are not already enabled
    if (!array_key_exists($extension, $target_enabled_modules)) {
      // Only include extensions that have code available (in the project cache)
      if (is_dir($fsm_context['project_cache'] . '/' . $project)) {
        // We could call drupal_parse_info_file directly, save for the fact that we are bootstrapped to the old version of Drupal, and would need to bootstrap the new site to use this. We will therefore use Drush's copy instead.
        $info = drush_site_upgrade_parse_info_file($fsm_context['project_cache'] . '/' . $project, $extension);
        if (!empty($info)) {
          $dependencies_map[$extension] = array_key_exists('dependencies', $info) ? $info['dependencies'] : array();
          $extensions_to_be_upgraded[$extension] = $extension;
        }
        else {
          drush_log(dt("No .info file could be found for !extension", array('!extension' => $extension)), 'warning');
        }
      }
    }
  }
  // If a given extension has any dependencies that are in the "to be upgraded" list,
  // then we will move it to the "not ready for upgrade" list.
  $extensions_ready_for_upgrade = array();
  $extensions_not_ready_for_upgrade = array();
  foreach ($extensions_to_be_upgraded as $extension) {
    $readyForUpgrade = TRUE;
    $hasDependencies = FALSE;
    $dependencies = $dependencies_map[$extension];
    foreach ($dependencies as $dependency) {
      $hasDependencies = TRUE;
      if (in_array($dependency, $extensions_to_be_upgraded)) {
        $readyForUpgrade = FALSE;
      }
    }
    if ($readyForUpgrade) {
      // If $hasDependencies is false, then put this on the head of the list; otherwise, put it at the end.
      // This insures that we will order extensions without dependencies ahead of extensions with dependencies)
      $key = $extension;
      $label = $extension;
      if (array_key_exists($extension, $label_map)) {
        $label = $label_map[$extension];
      }
      if ($hasDependencies) {
        $extensions_ready_for_upgrade[$key] = $label . " (Deps: " . implode(',', $dependencies) . ")";
      }
      else {
        $extensions_ready_for_upgrade = array_merge(array($key => $label), $extensions_ready_for_upgrade);
      }
    }
    else {
      $extensions_not_ready_for_upgrade[] = $extension . " (Deps: " . implode(',', $dependencies) . ")";
    }
  }
  // Filter out extensions that had problems on a previous pass
  $extensions_without_problems = array();
  $extensions_with_problems = array();
  foreach ($extensions_ready_for_upgrade as $extension => $label) {
    if (!array_key_exists($extension, $fsm_context['extensions_with_problems'])) {
      $extensions_without_problems[$extension] = $label;
    }
    else {
      $extensions_with_problems[$extension] = $label . ' [[' . $fsm_context['extensions_with_problems'][$extension]['message'] . ']]';
    }
  }
  // If there are no extensions without problems remaining (and ready for upgrade), 
  // but there still are extensions with problems, then un-set 'auto' mode
  // so that the user will be prompted about what to do next.
  if (empty($extensions_without_problems) && !empty($extensions_with_problems)) {
    $fsm_context['auto'] = FALSE;
  }
  // We will allow the user to select from modules that have not failed AND also retry
  // modules that have failed, if desired, but we will put the problem extensions on the end.
  $extensions_ready_for_upgrade = array_merge($extensions_without_problems, $extensions_with_problems);
  // Add a 'note' about the extensions that need to be upgraded, but
  // are not yet ready.
  if (!empty($extensions_not_ready_for_upgrade)) {
    drush_print(dt("Note: in addition to the modules below, the following still need to be upgraded, but cannot be started until their dependencies below are upgraded: ") . implode(',', $extensions_not_ready_for_upgrade));
  }
  $control['selection-options'] = $extensions_ready_for_upgrade;
  if (empty($extensions_to_be_upgraded)) {
    $control['prompt'] = dt("There are no more modules to be upgraded.");
    $control['selection-options']['#done'] = dt("All done; finish upgrade");
  }
  else {
    $control['prompt'] = dt("Please select a module to upgrade, or some other action below:");
    $control['selection-options']['#done'] = dt("Finish upgrade, leaving remaining modules un-upgraded");
  }
}

/**
 * Find the info file and parse it
 */
function drush_site_upgrade_parse_info_file($project_dir, $extension) {
  $files = drush_scan_directory($project_dir, '/^' . $extension . '\.info$/', array('.', '..', 'CVS'));
  if (empty($files)) {
    return array();
  }
  $info_file = array_shift(array_keys($files));
  // We could call drupal_parse_info_file directly, save for the fact that we are bootstrapped to the old version of Drupal, and would need to bootstrap the new site to use this. We will therefore use Drush's copy instead.
  $info = drush_drupal_parse_info_file($info_file);
  return $info;
}

function _drush_site_upgrade_prepare_module_upgrade(&$fsm_context, &$control) {
  $fsm_context['module_to_upgrade'] = $control['selection'];
  return TRUE;
}

function _drush_site_upgrade_show_module_upgrade_txt(&$fsm_context, &$control) {
  // TODO: the UPGRADE.txt for views is D7UPGRADE.txt
  $extension = $fsm_context['module_to_upgrade'];
  $project = $fsm_context['project_map'][$extension];
  $upgrade_txt = $fsm_context['project_cache'] . '/' . $project . '/UPGRADE.txt';
  if (is_file($upgrade_txt)) {
    $control['note'] = file_get_contents($upgrade_txt);
  }
  else {
    $control['note'] = dt("There is no UPGRADE.txt file in !extension", array("!extension" => $extension));
  }
  $control['prompt'] = dt("Please review the information above:");
  $control['selection-options']['_run'] = dt("Begin upgrade of module !module", array('!module' => $fsm_context['module_to_upgrade']));
  $control['selection-options']['#postpone'] = dt("Do not upgrade !module yet; select another one to do first", array('!module' => $fsm_context['module_to_upgrade']));
  return FALSE;
}

function _drush_site_upgrade_show_module_location(&$fsm_context, &$control) {
  $destination_core = $fsm_context['destination_core'];
  $extension = $fsm_context['module_to_upgrade'];
  $project = $fsm_context['project_map'][$extension];
  // TODO: Reconcile with code below
  $src = $fsm_context['project_cache'] . '/' . $project;
  $dest = $destination_core . '/sites/all/modules/' . $project;
  if (is_dir($src)) {
    $control['note'] .= "\n" . dt("Drush has a cached version of !module at !path.", array('!module' => $project, '!path' => $src));
  }
  $control['note'] .= "\n" . dt("The destination directory is !path", array('!path' => $dest));
}

function _drush_site_upgrade_verify_module_code_exists(&$fsm_context, &$control) {
  $destination_core = $fsm_context['destination_core'];
  $extension = $fsm_context['module_to_upgrade'];
  $project = $fsm_context['project_map'][$extension];
  $dest = $destination_core . '/sites/all/modules/' . $project;
  // TODO: We should validate that the directory at $dest is actually the right
  // version of the desired module (at least insure that the Drupal major verison matches).
  // For now, we will assume that the code in place is correct if it is there.
  // If the wrong thing is there, we should prompt the user and ask if it is
  // okay to delete it.
  return is_dir($dest);
}

function _drush_site_upgrade_get_module_code(&$fsm_context, &$control) {
  // TODO: if we haven't fetched this project already, then fetch it and mark it as already fetched.
  $destination_core = $fsm_context['destination_core'];
  $extension = $fsm_context['module_to_upgrade'];
  $project = $fsm_context['project_map'][$extension];
  // TODO: Kind of a bummer that we're going to need to reproduce logic in pm-download to
  // put modules and themes in the right location. Maybe there's a better way to do this?
  $src = $fsm_context['project_cache'] . '/' . $project;
  $dest = $destination_core . '/sites/all/modules/' . $project;
  // n.b. _drush_site_upgrade_verify_module_code_exists should insure that $dest is empty
  return drush_copy_dir($src, $dest, FALSE);
}

function _drush_site_upgrade_enable_module(&$fsm_context, &$control) {
  return _drush_site_upgrade_enable_specified_module($fsm_context, &$control, $fsm_context['module_to_upgrade']);
}

function _drush_site_upgrade_enable_content_migrate(&$fsm_context, &$control) {
  return _drush_site_upgrade_enable_specified_module($fsm_context, &$control, 'content_migrate');
}

function _drush_site_upgrade_enable_specified_module(&$fsm_context, &$control, $specified_module) {
  $result = drush_invoke_process($fsm_context['target_key'], 'pm-enable', array($specified_module), array(), array('integrate' => TRUE));
  $resultcode = TRUE;
  $module_was_enabled = is_array($result['object']) && array_key_exists($specified_module, $result['object']);
  // If we could successfully enable the module, clear it from the 'extensions with problems'
  // list, if it was included there.  If the module could not be enabled, then take it out of the list.
  // TODO: If the module was already enabled, that isn't really a problem. Double-check for that before signalling a failure
  if ($module_was_enabled) {
    unset($fsm_context['extensions_with_problems'][$specified_module]);
  }
  else {
    $fsm_context['extensions_with_problems'][$specified_module] = array('message' => dt("Error: failed to enable."));
    return FALSE;
  }
  return TRUE;  
}

function _drush_site_upgrade_fix_permissions(&$fsm_context, $control) {
  // The only thing we will do for now is make an attempt at making the 'files' directory writable
  // to clear up one more potential problem with the status report.
  $result = drush_invoke_process($fsm_context['target_key'], "drupal-directory", array("%files"));
  $files_dir = $result['object'];
  chmod(dirname($files_dir), 0777);
  chmod($files_dir, 0777);
  // TODO: The status report still says:
  // "The directory temp does not exist. You may need to 
  // set the correct directory at the file system       
  // settings page or change the current directory's    
  // permissions so that it is writable."
  // Need to create the temp directory. 
  return TRUE;
}

function _drush_site_upgrade_run_cron(&$fsm_context, $control) {
  $result = (drush_invoke_process($fsm_context['target_key'], 'core-cron', array(), array(), array('interactive' => TRUE)) == 0);
  return $result;
}

function _drush_upgrade_site_download_next_version_drupal(&$fsm_context, $control) {
  $source_version = $fsm_context['source_version'];
  $target_version = $fsm_context['target_version'];
  $target_key = $fsm_context['target_key'];
  // Should be impossible to get here without a destination core
  if (array_key_exists('destination_core', $fsm_context)) {
    $destination_core = $fsm_context['destination_core'];
    $destination_conf_path = $fsm_context['destination_conf_path'];

    // Skip the download if there are already files at the destination core directory
    if (!is_dir($destination_core . '/includes/bootstrap.inc')) {
      drush_upgrade_download_drupal($target_version, $destination_core);
      if (drush_get_error()) return FALSE; // Early exit if we see an error.
    }
  }
  return TRUE;
}

function _drush_upgrade_site_make_settings_writable(&$fsm_context, $control) {
  if (array_key_exists('destination_core', $fsm_context) && array_key_exists('target_alias', $fsm_context)) {
    $destination_core = $fsm_context['destination_core'];
    $destination_conf_path = $fsm_context['destination_conf_path'];
    $fsm_context['settings_destination'] = $destination_core . '/' . $destination_conf_path . '/settings.php';
    return drush_upgrade_copy_settings($fsm_context['target_alias'], $fsm_context['settings_destination']);
  }
  return TRUE;
}

function _drush_upgrade_site_copy_database_and_disable_contrib(&$fsm_context, $control) {
  $target_key = $fsm_context['target_key'];
  $target_alias = $fsm_context['target_alias'];
  $destination_core = $fsm_context['destination_core'];
  $destination_conf_path = $fsm_context['destination_conf_path'];
  $fsm_context['settings_destination'] = $destination_core . '/' . $destination_conf_path . '/settings.php';
  // Copy source database to target database. The source DB is not changed.
  // Always set 'common' at minimum. Sites that want other can create other key in drushrc.php.
  if (!drush_get_option('structure-tables-key')) {
    drush_set_option('structure-tables-key', 'common');
  }
  // Empty or create the target database as needed.
  drush_sql_empty_db(drush_sitealias_get_db_spec($target_alias));
  // We still have DRUSH_AFFIRMATIVE set from above, so this will not prompt.
  // TODO: redo this as drush_invoke_process
  drush_invoke_process('@self', 'sql-sync', array('@self', $target_key), array('yes' => TRUE), array('integrate' => TRUE));
  if (drush_get_error()) return FALSE; // Early exit if we see an error.
  // When we first copy the database, its version will be the same as the source site.
  // We will advance its version by one after we run updatedb.
  $fsm_context['target_database_version'] = $fsm_context['source_version'];
  
  if (!empty($fsm_context['contrib_extensions'])) {
    if (!_drush_upgrade_site_create_transition_site($fsm_context, $control)) {
      return FALSE;
    }
    $result = (drush_invoke_process($fsm_context['modify_site'], 'site-upgrade-prepare', $fsm_context['contrib_extensions'], array('uninstall' => implode(',', $fsm_context['uninstall_extensions']), 'yes' => TRUE), array('interactive' => TRUE)) == 0);
    if ($result === FALSE) return FALSE;
  }

  return TRUE;
}

function _drush_upgrade_site_verify_default_settings_removed(&$fsm_context, $control) {
  return !file_exists($fsm_context['destination_core'] . '/' . $fsm_context['destination_conf_path'] . '/default.settings.php');
}

function _drush_upgrade_site_remove_default_settings(&$fsm_context, $control) {
  drush_op('unlink', $fsm_context['destination_core'] . '/' . $fsm_context['destination_conf_path'] . '/default.settings.php');
  return TRUE;
}

function _drush_site_upgrade_copy_files(&$fsm_context, $control) {
  $source_site = '@self';
  $target_site = $fsm_context['target_key'];
  $result = drush_invoke_process('@self', 'core-rsync', array($source_site . ':%files', $target_site . ':%files'), array('yes' => TRUE));
  return ($result['error_status'] == 0);
}

/**
 * The 'transition site' is a new multi-site entry created in the sites
 * folder of the *source* site that points at the database for the
 * *destination* site.  After the source database is copied to the target
 * database, it will still be in the format of the source site's version
 * of Drupal.  Drush will bootstrap the transition site in order to use
 * the source site's Drupal code to pm-disable and pm-uninstall modules
 * in the destination database without affecting those modules in the
 * source site's database.
 *
 * After updatedb is run for the first time on the target site's database,
 * the transition site will not be usable any longer, because the database
 * will have been converted to the format of the target site's version of
 * Drupal.  Once this happens, it won't be possible to bootstrap the target
 * database with the source site's Drupal code.  Therefore, it will not
 * be possible to pm-uninstall any modules after updatedb is ran.  If you
 * decide to do this, you'll need to go back, copy the source database
 * again, pm-uninstall modules as desired, and then re-run updatedb again.
 */
function _drush_upgrade_site_create_transition_site(&$fsm_context, $control) {
  //var_export($fsm_context['target_alias']);
  $target_alias_databases = sitealias_get_databases_from_record($fsm_context['target_alias']);
  $fsm_context['modify_site_conf_path'] = NULL;

  // Make an alias record that uses the CODE from @self and the DATABASE from $target.
  // Since we just did an sql-sync from @self to @target, we can use this hybrid specification
  // to do manipulations on the target database before runing updatedb.  In brief, we are going
  // to disable all contrib modules to prevent problems with updatedb.
  $fsm_context['modify_site'] = array (
    'root' => DRUPAL_ROOT,
    'uri' => $target_alias_databases['default']['default']['database'],
  );

  if (!drush_get_context('DRUSH_SIMULATE')) {
    // In theory, if the sql-sync worked, this should never be empty.
    if (empty($fsm_context['modify_site']['uri'])) {
      return drush_set_error('DRUSH_UPGRADE_DATABASE_SPEC_UNKNOWN', dt('Failed to look up database spec for @target', array('@target' => $target_key)));
    }
    $fsm_context['modify_site_conf_path'] = dirname(conf_path()) . '/' . $fsm_context['modify_site']['uri'];
    $modify_site_settings = $fsm_context['modify_site_conf_path'] . '/settings.php';
    drush_log('Set up a transient site by copying ' . $fsm_context['settings_destination'] . ' to ' . $modify_site_settings, 'debug');
    if (!file_exists($modify_site_settings)) {
      if ((drush_mkdir($fsm_context['modify_site_conf_path'], TRUE) === FALSE) || drush_op('copy', $fsm_context['settings_destination'], $modify_site_settings) !== TRUE) {
        return drush_set_error('DRUSH_UPGRADE_COULD_NOT_DISABLE', dt("Could not create a temporary multisite "));
      }
    }
  }
  // Debugging:
  // $result = (drush_invoke_process($fsm_context['modify_site'], 'sql-conf', array('integrate' => TRUE)) == 0);
  return TRUE;
}

/**
 * After updatedb is ran on the database, you might as well get rid of
 * the transition site.  @see _drush_upgrade_site_create_transition_site
 */
function _drush_upgrade_site_delete_transition_site(&$fsm_context, $control) {
  // Delete the temporary site now that we're done with it.
  if (isset($fsm_context['modify_site_conf_path'])) {
    drush_delete_dir($fsm_context['modify_site_conf_path']);
    unset($fsm_context['modify_site_conf_path']);
  }
}

function _drush_upgrade_site_updatedb(&$fsm_context, $control) {
  // After updatedb runs, set the target database version
  $fsm_context['target_database_version'] = $fsm_context['target_version'];
  return TRUE;
}

function _drush_upgrade_site_backup_after_updatedb(&$fsm_context, $control) {
  return TRUE;
}

/**
 * Refresh information about enabled modules on every run, in case user changed
 * something.
 */
function drush_upgrade_gather_information(&$fsm_context) {
  $source_version = $fsm_context['source_version'];
  $target_version = $fsm_context['target_version'];
  $target_key = $fsm_context['target_key'];

  drush_print();
  drush_print(dt("Gathering information about core and contrib modules..."));

  // Refresh the extension info every time until the database is
  // sync'ed to the target site. After that point, the extension info
  // is frozen, as changes to the source site will no longer affect the
  // target site.
  if (!array_key_exists('extension_info', $fsm_context) || ($fsm_context['target_database_version'] == 0)) {
    $fsm_context['extension_info'] = drush_get_extensions();

    // Get a list of enabled contrib extensions in source site.
    $values = drush_invoke_process('@self', 'pm-list', array(), array('status'=>'enabled','no-core'=>TRUE), array('integrate' => FALSE, 'override-simulated' => TRUE));
    if ($values['error_status'] != 0) return FALSE;
    $fsm_context['contrib_extensions'] = array_keys($values['object']);

    // Get a list of enabled core extensions in source site.
    $values = drush_invoke_process('@self', 'pm-list', array(), array('status'=>'enabled','core'=>TRUE), array('integrate' => FALSE, 'override-simulated' => TRUE));
    if ($values['error_status'] != 0) return FALSE;
    $fsm_context['core_extensions'] = array_keys($values['object']);
  }

  // Make a list of modules that are not available to be enabled in the target
  // version of Drupal, either because they have not been ported yet, or because
  // they have been rolled into core.
  $fsm_context['unavailable_extensions'] = array();

  // Get the list of modules the user would like to uninstall (if any).
  if ($fsm_context['uninstall_extensions'] == "all") {
    $fsm_context['uninstall_extensions'] = $fsm_context['contrib_extensions'];
  }
  elseif (!is_array($fsm_context['uninstall_extensions'])) {
    $fsm_context['uninstall_extensions'] = explode(',', $fsm_context['uninstall_extensions']);
  }

  // WARN:  Tell the user about any special situations that might exist with contrib modules.

  $fsm_context['project_download_list'] = array();
  $fsm_context['project_info_map'] = array();
  $fsm_context['contrib_to_upgrade'] = array();
  if (!empty($fsm_context['contrib_extensions']) && !$fsm_context['core-only']) {
    // Make a list of all of the extensions to download.  We will
    // download everything in the contrib extension list, but we
    // will skip projects that already exist.
    $fsm_context['special_projects'] = drush_upgrade_project_map($target_version);
    $in_core = array();
    $fsm_context['special_warning'] = array();
    foreach ($fsm_context['contrib_extensions'] as $extension_name) {
      // Only check extensions that are NOT in our uninstall list.
      if (!in_array($extension_name, $fsm_context['uninstall_extensions']) && array_key_exists($extension_name, $fsm_context['extension_info'])) {
        if (empty($fsm_context['extension_info'][$extension_name]->info['project'])) {
          drush_log(dt('No project information could be found for the module !module. Enable git_deploy project if you are using Git clones as projects.', array('!module' => $extension_name)), 'warning');
        }
        else {
          $project = $fsm_context['extension_info'][$extension_name]->info['project'];
          $fsm_context['project_info_map'][$project]['name'] = $fsm_context['extension_info'][$extension_name]->info['name'];
          $fsm_context['project_info_map'][$project]['version'] = $fsm_context['extension_info'][$extension_name]->info['version'];
          // Check our lookup table to see if a project has been renamed.
          if (array_key_exists($project, $fsm_context['special_projects']['project-remap'])) {
            $project = $fsm_context['special_projects']['project-remap'][$project];
          }
          // If the module has been rolled into core in the next major release of
          // Drupal, then we do not need to download it.  Add it to an array for
          // reporting purposes.
          if ((in_array($project, $fsm_context['special_projects']['projects-in-core'])) || (in_array($extension_name, $fsm_context['special_projects']['modules-in-core']))) {
            $in_core[$extension_name] = $extension_name;
            // Might some of these need to be enabled? No, I think we just skip the 'disable' step for this module, since it is now core
            $fsm_context['unavailable_extensions'][] = $extension_name;
          }
          // n.b.: Previously, we would skip $extension_name if $project existed in destination: (!isset($destination_core) || (!is_dir($destination_core . '/sites/all/modules/' . $project) && !is_dir($destination_core . '/' . $destination_conf_path . '/modules/' . $project)))) {
          // We have a different way to handle this now.
          else {
            $fsm_context['contrib_to_upgrade'][] = $extension_name;
            if ($fsm_context['extension_info'][$extension_name]->type == 'module') {
              $fsm_context['project_download_list'][$project][] = $extension_name;
            }
            // If there is a special warning about a project, then add it
            // to the warning list for later reporting.
            if (array_key_exists($project . ':' . $extension_name, $fsm_context['special_projects']['warning'])) {
              $fsm_context['special_warning'][$project] = $fsm_context['special_projects']['warning'][$project . ':' . $extension_name];
            }
            elseif ((array_key_exists($project, $fsm_context['special_projects']['warning'])) && (!array_key_exists($project, $fsm_context['special_warning']))) {
              $fsm_context['special_warning'][$project] = $fsm_context['special_projects']['warning'][$project];
            }
          }
        }
      }
    }
  }
  return TRUE;
}

function drush_upgrade_check_modules_and_themes(&$fsm_context, $control) {
  // Presume we are ready to go
  $ready_to_upgrade = TRUE;
  $source_version = $fsm_context['source_version'];
  $target_version = $fsm_context['target_version'];
  $target_key = $fsm_context['target_key'];
  if (array_key_exists('destination_core', $fsm_context)) {
    $destination_core = $fsm_context['destination_core'];
    $destination_conf_path = $fsm_context['destination_conf_path'];
  }
  if (empty($fsm_context['contrib_extensions'])) {
    drush_print();
    drush_print(dt("There are no enabled contrib extensions; Drush will upgrade Drupal core."));
  }
  elseif ($fsm_context['core-only']) {
    drush_print();
    drush_print(dt("--core-only option specified; Drush will upgrade Drupal core only."));
  }
  else {
    $rows[] = array(dt('Project'), dt('Title'), dt('Installed'), dt('Available'), dt('Status'));
    // Consider each project from the contrib extensions and check with PM to see if there is
    // a recommended release available to download.  If there is NO release available, then
    // we are not ready to upgrade (but still can, without that project); if there is no RECOMMENDED
    // release, then we might not be ready to upgrade (but still can, with a non-recommended release).
    if (!empty($fsm_context['project_download_list'])) {
      $result = drush_invoke_process('@none', 'pm-releases', array_keys($fsm_context['project_download_list']), array('default-major' => $target_version), array('integrate' => FALSE, 'override-simulated' => TRUE, 'log' => FALSE));
      $project_releases = $result['object'];
      foreach ($fsm_context['project_download_list'] as $project => $extension_list) {
        if (!array_key_exists($project, $project_releases) || empty($project_releases[$project]['releases'])) {
          $fsm_context['unavailable_extensions'] = array_merge($fsm_context['unavailable_extensions'], $extension_list);
          $ready_to_upgrade = FALSE;
          $available_version = '-';
          $status = dt("NO !major_version RELEASES", array('!major_version' => $target_version . ".x"));
        }
        else {
          $all_versions = array_keys($project_releases[$project]['releases']);
          $available_version = $all_versions[0];
          if (empty($project_releases[$project]['recommended'])) {
            $ready_to_upgrade = 'maybe';
            $status = implode(',', $project_releases[$project]['releases'][$available_version]['release_status']);
          }
          else {
            $status = dt('Recommended');
          }
        }
        $title = $fsm_context['project_info_map'][$project]['name'];
        $installed_version = $fsm_context['project_info_map'][$project]['version'];
        $rows[] = array($project, $title, $installed_version, $available_version, $status);
      }
    }
    drush_print_table($rows);

    // Print out some messages about projects that migrated  to core, or modules that will require special processing.
    if (!empty($in_core)) {
      drush_log(dt('The following contrib modules were enabled in your Drupal site, but are now standard in core: !in_core.  These modules may need to be reconfigured after the upgrade is complete.', array('!in_core' => implode(', ', $in_core))), 'ok');
    }
    foreach ($fsm_context['special_warning'] as $project => $warning) {
      if ($warning === TRUE) {
        $warning = 'Please see !project_page and !source for more information on how to do this.';
      }
      if ($warning === FALSE) {
        $warning = 'So far there is no indication of when a migration path will be provided.  Check !project_page for updates.';
        $ready_to_upgrade = 'maybe';
      }
      drush_log(dt("You are using the project !project, which requires data migration or other special processing.  $warning", array('!project' => $project, '!project_page' => 'http://drupal.org/project/' . $project, '!source' => $fsm_context['special_projects']['source'])), 'warning');
    }
  }
  if (drush_upgrade_site_upgrade_check_parameters($target_key) === FALSE) {
    return FALSE;
  }

  // Summarize whether or not there is a good chance that the site can be upgraded.
  if ($ready_to_upgrade !== TRUE) {
    drush_print();
    drush_log(dt("Based on the contrib modules enabled in this site, it is possible that the site-upgrade command might fail.  See warnings above."), (($ready_to_upgrade === FALSE) ? 'warning' : 'notice'));
  }

  return TRUE;
}

/**
 * http://drupal.org/node/895314 lists projects that are now in
 * core; it also has a list of projects that require special handling.
 * Keep a record here too.
 *
 * @param $target_version
 * The version of Drupal being upgraded to.
 * @return @array
 *   - source              URL to the page where more information about this upgrade can be found.
 *   - target-version      The version of Drupal being upgraded to.
 *   - projects-in-core    An array containing projects that were once contrib that are now in core.
 *   - modules-in-core     An array containing modules that were once in contrib that are now in core.
 *                         Use 'modules-in-core' in place of 'projects-in-core' only if there is a
 *                         project where only some of its subcomponents were moved to core.
 *   - warning             An array of warning messages to display to the user related to this upgrade.
 *                         The key should be the name of the project that the warning applies to.
 *                         use 'project:module' as the key if the warning only applies when the given
 *                         module in the project is enabled; otherwise, the warning will be shown whenever
 *                         any module in the specific project is enabled.  The value can either be the
 *                         warning string to display, or TRUE to tell the user to see the project page
 *                         for information on the migration path, or FALSE to tell the user that there
 *                         currently is no migration path for the given project.
 */
function drush_upgrade_project_map($target_version) {
  $result = array(
    'source' => '',
    'target-version' => $target_version,
    'projects-in-core' => array(),
    'modules-in-core' => array(),
    'warning' => array(),
  );
  if ($target_version == 7) {
    $result = array(
      'source' => 'http://drupal.org/node/895314',
      'projects-in-core' => array(
        'abssrc',
        'admin_hover',
        'adminrole',
        'ahah_helper',
        'autoload',
        'automaticmenu',
        'automenu',
        'auto_menutitle',
        'block_edit',
        'block_node_visibility',
        'blocks404',
        'canonical_url',
        'checkbox_validate',
        'comment_display',
        'config_perms',
        'ctm',
        'dbtng',
        'documentation',
        'drupal_queue',
        'edit_term',
        'elements',
        'element_themehook',
        'filefield',
        'filter_default',
        'hook_file',
        'imageapi',
        'imagecache',
        'imagefield',
        'input_format_permissions',
        'jq',
        'jqp',
        'jquery_cookie',
        'jquery_form_update',
        'jquery_ui',
        'jsalter',
        'login_security',
        'menuless_nodetype',
        'nodepreview_by_type',
        'parallel',
        'permissions_api',
        'phpass',
        'plugin_manager',
        'plugins',
        'poormanscron',
        'preview',
        'rdf',
        'shortlink',
        'simplecdn',
        'simpletest',
        'storage_api',
        'tar',
        'taxidermy',
        'tinymce_ahah',
        'tinymce_dragdrop',
        'transaction',
        'upload_element',
        'upload_preview',
        'url_alter',
        'user_cancellation',
        'user_default_filter',
        'user_delete',
        'vertical_tabs',
        'view_unpublished',
        'vocabperms',
        'wysiwygcck',
      ),
      'modules-in-core' => array(
        // 'date' project is still contrib, but date_timezone, one of its modules, has moved to core. See http://drupal.org/node/868028.
        'date_timezone',
      ),
      // If a project changes name, then an entry
      // 'oldprojectname' => 'newprojectname' should be placed in this map.
      // 'project-remap' should only be used if the extensions in the project
      // all exist with the same name in the new project name.  If modules change
      // name, hten 'module-remap' and 'module-project-relocation' should be
      // used instead.
      'project-remap' => array(),
      // If an extension changes name, then an entry
      // 'oldmodulename' => 'newmodulename' should be placed in this map.
      // By default, it is assumed that the new module name exists in the
      // same project as before.  If this is not the case, then an entry
      // in the 'module-project-relocation' array should also be used.
      'module-remap' => array(
        'content' => 'cck',
        'content_copy' => 'bundle_copy',
        'content_permissions' => 'field_permissions',
        'fieldgroup' => 'field_group',
        // TODO: nodereference and userreference could optionally be migrated to entityreference. Which has the better upgrade path?
        'nodereference' => 'node_reference',
        'userreference' => 'user_reference',
        'views_export' => 'bulk_export',
        'optionwidgets' => 'options',
      ),
      // The module-project-relocation array can be used to record modules
      // that moved to a new project in Drupal 7.  Entries should be in
      // the form 'modulename' => 'newprojectname'.  The module name should
      // be the -new- module name for modules that appear in the module-remap
      // array, or the existing module names for modules that did not change name.
      'module-project-relocation' => array(
        'bundle_copy' => 'bundle_copy',
        'field_permissions' => 'field_permissions',
        'field_group' => 'field_group',
        'node_reference' => 'references',
        'user_reference' => 'references',
        'entityreference' => 'entityreference',
        'bulk_export' => 'ctools',
        'options' => 'drupal',
      ),
      'warning' => array(
        'token' => dt('In Drupal 7, the contrib token module handles UI, as well as field and profile tokens; all other functionality has been migrated to core.'),
        'cck' => TRUE,
        'content_taxonomy' => dt('The project page for this module (!project_page) says that an upgrade path will be provided for this module at a future time.'),
        'date:date_api' => dt("The d6 version of the date_api module in the date project defines a table called date_formats, which is also defined by system/system.install schema in d7.  If this problem has not been fixed yet, then the updatedb function will fail, and it will not be possible to upgrade to d7.  If this happens, disable and uninstall the date_api module before running site-upgrade (i.e. add '--uninstall=date_api' to site-upgrade call).  See http://drupal.org/node/1013034."),
        'field_taxonomy' => FALSE,
        'filefield' => dt('Data migration for this module will be provided by the Content Migrate submodule of cck.  Enable content_migrate after upgrading; see http://drupal.org/node/781088.'),
        'imagefield' => dt('Data migration for this module will be provided by the Content Migrate submodule of cck.  Enable content_migrate after upgrading; see http://drupal.org/node/781088.'),
        'taxonomy_delegate' => FALSE,
        'taxonomy_intro' => FALSE,
        'term_fields' => FALSE,
      ),
    );
  }
  drush_command_invoke_all_ref('drush_upgrade_project_map_alter', $result);

  return $result;
}

/**
 * Download the upgraded version of Drupal for site-upgrade.
 */
function drush_upgrade_download_drupal($target_version, $destination_core) {
  // Fetch target core and place as per target alias root.
  drush_set_option('destination', dirname($destination_core));
  drush_set_option('drupal-project-rename', basename($destination_core));

  // No need for version control in this command.
  drush_set_option('version-control', 'backup');

  if (drush_get_context('DRUSH_SIMULATE')) {
    drush_log(dt("Simulated download of drupal-!version", array('!version' => $target_version)));
  }
  else {
    // TODO: convert to drush_invoke_process; set --yes
    drush_invoke('pm-download', 'drupal-' . $target_version);
    if (drush_get_error()) return FALSE; // Early exit if we see an error.

    // Check and see if there is a Drupal site at the target.
    if (!file_exists($destination_core . '/includes/bootstrap.inc')) {
      return drush_set_error('DRUSH_UPGRADE_NO_DRUPAL', dt('Drupal could not be downloaded to the target directory, @root.  Move existing content out of the way first.', array('@root' => $target_alias['root'])));
    }
  }
}

/**
 * Copy the settings.php file from the source site to the target site,
 * and fix it up so that it will have its own database settings.
 */
function drush_upgrade_copy_settings(&$target_alias, $settings_destination) {
  $settings_destination_folder = dirname($settings_destination);

  // Create sites subdirectory in target if needed.
  $settings_source = conf_path() . '/settings.php';
  if (!file_exists($settings_destination_folder)) {
    if (!drush_op('mkdir', $settings_destination_folder) && !drush_get_context('DRUSH_SIMULATE')) {
      return drush_set_error('DRUSH_UPGRADE_MKDIR_FAILED', dt('Failed to create directory @settings_destination', array('@settings_destination' => $settings_destination_folder)));
    }
  }

  // Copy settings.php to target.
  if (!file_exists($settings_destination)) {
    if (!drush_op('copy', $settings_source, $settings_destination) && !drush_get_context('DRUSH_SIMULATE')) {
      return drush_set_error('DRUSH_UPGRADE_COPY_FAILED', dt('Failed to copy @source to  @dest', array('@source' => $settings_source, 'dest' => $settings_destination)));
    }
  }
  // Append new $db_url with new DB name in target's settings.php.
  return drush_upgrade_fix_db_url($target_alias, $settings_destination);
}

/**
 * Replace db_url with DB name from target. Write a $databases record to settings.php.
 */
function drush_upgrade_fix_db_url(&$target_alias, $settings_destination) {
  $old_url = $GLOBALS['db_url'];
  if (is_array($old_url)) {
    $old_url = $old_url['default'];
  }
  $old_databases = empty($GLOBALS['databases']) ? drush_sitealias_convert_db_from_db_url($old_url) : $GLOBALS['databases'];

  $target_alias_databases = sitealias_get_databases_from_record($target_alias);
  $database_name = $target_alias_databases['default']['default']['database'];
  if (empty($database_name)) {
    $database_name = preg_replace("/[^a-zA-Z0-9\s]/", "", $target_alias['#name']) . "db";
    drush_log(dt("No database name specified; defaulting to !dbname", array("!dbname" => $database_name)), 'notice');
  }

  $append = "\n# Added by drush site-upgrade.";
  if (drush_drupal_major_version() <= 6) {
    $new_url = substr($old_url, 0, strrpos(trim($old_url), '/')) . '/'. $database_name;
    $append .= "\n# New db_url:\n\$db_url = '$new_url';";
    $databases = drush_sitealias_convert_db_from_db_url($new_url);
    // Handle db prefix
    if (isset($GLOBALS['db_prefix'])) {
      $append .= "\n\$db_prefix = '" . $GLOBALS['db_prefix'] . "';";
      $databases['default']['default']['prefix'] = $GLOBALS['db_prefix'];
    }
  }
  else {
    $databases = $GLOBALS['databases'];
    $databases['default']['default']['database'] = $target_alias_databases['default']['default']['database'];
  }
  $append .= "\n# New databases array:\n" . '$databases = ' . var_export($databases, TRUE) . ';';
  // Caching the database record in the alias record allows sql-sync to work
  // before updatedb is called. updatedb is what converts from a db_url to a
  // DBTNG array; this conversion is required by sql-sync.
  drush_sitealias_cache_db_settings($target_alias, $databases);

  // Also append the new configuration options to the end of settings.php.
  return drush_op('file_put_contents', $settings_destination, $append, FILE_APPEND) !== FALSE;
}
