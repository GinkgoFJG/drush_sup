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
    ),
    'examples' => array(
      'drush site-upgrade @onward' => 'Upgrade from the current site to the site specified by @onward alias.',
    ),
    'options' => array(
      'straightforward' => 'Run all of the straightforward steps automatically; only prompt at important steps.',
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
 * in the stages array is the state name; the first state is always
 * START, and the last state is always STOP.
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
 *  - callback:
 *
 *    The name of the function callback that Drush will call to perform the
 *    step.  If not specified, Drush will say: "Drush cannot perform this step;
 *    you must do it manually."
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
 *    next state of the FSM to advance to.  TRUE is mapped to 'ok',
 *    FALSE is mapped to 'error'.  Other return values are possible,
 *    provided that they align with an entry in the 'next' array.
 *
 *    If the value of the selected item in the next array is an array,
 *    then the user will be prompted as to which step to perform next.  Each
 *    item of the array will contain a message to show the user in the prompt
 *    (the VALUE), and the name of the next state to go to if the option is 
 *    selected (the KEY).  Note that single-item arrays also make sense here, 
 *    as this will cause Drush to prompt to cancel / continue.
 *
 *  - selection-options:
 *
 *    Provides the list of prompts to present to the user.
 *
 *      'run' => 'Do it for me.'
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
 *    is used to replace the prompt for the 'run' item in the selection options.
 *
 *  - optional:
 *
 *    If specified, the user will be advised that this step is optional
 *    (does not always need to be done).
 *
 *  - unnecessary:
 *
 *    If specified, the user will be advised that this step is not necessary
 *    (does not apply when using the Drush workflow).
 *
 *  - straightforward:
 *
 *    This step does not take much thought or consideration on the part
 *    of the user.  Straightforward steps can be run automatically
 *    (without prompting) with the --straightforward option.
 *    Unnecessary steps are automatically considered straightforward.
 *
 */
function _drush_upgrade_site_upgrade_stages() {
  $stages['START'] = array(
    'next' => 'UPGRADE.txt Major Upgrade Step 1'
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
    'required' => 'Show upgrade readiness report.',
    'straightforward' => TRUE,
    'next' => array(
      'ok' => array(
        '#prompt' => 'Please review the report above.  You may want to uninstall modules that are not ready to upgrade yet.  What would you like to do next?',
        'UPGRADE.txt Major Upgrade Step 2' => "Begin upgrade",
        'UPGRADE.txt Major Upgrade Step 1' => "Run report again",
      ),
      'error' => STOP,
    ),
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
    // TODO: we could use pm-updatecode here to insure that Drupal is up to date.
    // Do we update all of contrib to the latest recommended release too?
    'selection-options' => array('#skip' => 'I have updated to the latest available version of Drupal 6.x.'),
    'next' => 'UPGRADE.txt Major Upgrade Step 3',
  );
  $stages['UPGRADE.txt Major Upgrade Step 3'] = array(
    'description' =>
      "
        3. Log in as user ID 1 (the site maintenance user).
      ",
    'unnecessary' => 'You will not need to log in if Drush is used to perform all upgrade steps for you.',
    'next' => 'UPGRADE.txt Major Upgrade Step 4',
  );
  $stages['UPGRADE.txt Major Upgrade Step 4'] = array(
    'description' =>
      "
        4. Go to Administer > Site configuration > Site maintenance. Select
           \"Off-line\" and save the configuration.
      ",
    'next' => 'UPGRADE.txt Major Upgrade Step 5',
  );
  $stages['UPGRADE.txt Major Upgrade Step 5'] = array(
    'description' =>
      "
        5. Go to Administer > Site building > Themes. Enable \"Garland\" and select it as
           the default theme.
      ",
    'straightforward' => TRUE,
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
    'unnecessary' => 'Drush will automatically disable all non-core modules in the target site without affecting the source site.',
    'next' => 'UPGRADE.txt Major Upgrade Step 7',
  );
  $stages['UPGRADE.txt Major Upgrade Step 7'] = array(
    'description' =>
      "
        7. On the command line or in your FTP client, remove the file

             sites/default/default.settings.php
      ",
    'straightforward' => TRUE,
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
    'next' => 'UPGRADE.txt Major Upgrade Step 9',
  );
  $stages['UPGRADE.txt Major Upgrade Step 9'] = array(
    'description' =>
      "
        9. If you uninstalled any modules, remove them from the sites/all/modules and
           other sites/*/modules directories. Leave other modules in place, even though
           they are incompatible with Drupal 7.x.
      ",
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
    'straightforward' => TRUE,
    'next' => 'UPGRADE.txt Major Upgrade Step 11',
  );
  $stages['UPGRADE.txt Major Upgrade Step 11'] = array(
    'description' =>
      "
        11. Re-apply any modifications to files such as .htaccess or robots.txt.
      ",
    'optional' => "If you had no modifications to .htaccess or robots.txt, you don't need to do anything here.",
    'next' => 'UPGRADE.txt Major Upgrade Step 12',
  );
  $stages['UPGRADE.txt Major Upgrade Step 12'] = array(
    'description' =>
      "
        12. Make your settings.php file writeable, so that the update process can
           convert it to the format of Drupal 7.x. settings.php is usually located in

             sites/default/settings.php
      ",
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
    'next' => 'UPGRADE.txt Major Upgrade Step 14',
  );
  $stages['UPGRADE.txt Major Upgrade Step 14'] = array(
    'description' =>
      "
        14. Backup your database after the core upgrade has run.
      ",
    'next' => 'UPGRADE.txt Major Upgrade Step 15',
    'unnecessary' => 'Drush automatically backs up the code and database every time updatedb runs',
  );
  $stages['UPGRADE.txt Major Upgrade Step 15'] = array(
    'description' =>
      "
        15. Replace and update your non-core modules and themes, following the
            procedures at http://drupal.org/node/948216
      ",
    'next' => 'UPGRADE.txt Major Upgrade Step 16',
  );
  $stages['UPGRADE.txt Major Upgrade Step 16'] = array(
    'description' =>
      "
        16. Go to Administration > Reports > Status report. Verify that everything is
            working as expected.
      ",
      // TODO: 'optional' => 'Drush can show you this information now via the `status report` command.',
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
 * Do some sanity checks to make sure that we are ready to perform an upgrade, and
 * that the command is being called with reasonable-looking parameters.
 */
function drush_upgrade_site_upgrade_check_parameters($target_key = NULL) {
  if (empty($target_key)) {
    return drush_set_error('DRUSH_UPGRADE_NO_TARGET', dt('Missing argument: target'));
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

/**
 * Main command hook for site-upgrade.
 *
 * This runs bootstrapped to the SOURCE site.
 */
function drush_upgrade_site_upgrade($target_key = NULL) {
  // PREPARE:  Find the target version and determine the contrib projects and enabled modules installed.

  $source_version = drush_drupal_major_version();
  $target_version = $source_version + 1;
  if (isset($target_key)) {
    $target_alias = drush_sitealias_get_record($target_key);
    if (!empty($target_alias)) {
      $destination_core = $target_alias['root'];

      $destination_conf_path = conf_path();
      if (drush_get_option('force-sites-default')) {
        $destination_conf_path = 'sites/default';
      }
    }
  }

  // Cache our main variables in the FSM context
  $fsm_context['source_version'] = $source_version;
  $fsm_context['target_version'] = $target_version;
  $fsm_context['target_key'] = $target_key;
  $fsm_context['target_alias'] = $target_alias;
  $fsm_context['straightforward'] = drush_get_option('straightforward', FALSE);
  
  // TODO: Look up appropriate upgrade stages for the version of Drupal being upgraded.
  $stages = _drush_upgrade_site_upgrade_stages();
 
  // TODO: check to see if this is a restart of an earlier run
  // (maybe prompt about that), and restore the cached contexts
  // if it is.  We will look for a well-known file at the
  // destination root directory.
    
  // Start up the FSM and run the upgrade
  _drush_upgrade_site_state_machine($fsm_context, $stages, $stages['START']['next']);

  // TODO: if we're not finished with the upgrade, cache the
  // Drush contexts so that we can restart again later.
}

function _drush_upgrade_site_state_machine($fsm_context, $stages, $initial_state) {
  $state = $initial_state;
  $fsm_context += array(
    'straightforward' => FALSE,
  );
  while ($state != 'STOP') {
    $control = $stages[$state];
    $control += array(
      'description' => '',
      'required' => '',
      'optional' => '',
      'unnecessary' => '',
      'straightforward' => FALSE,
      'callback' => NULL,
      'prompt' => 'What would you like to do?',
      'selection-options' => array(),
      'next' => 'STOP',
    );
    // TODO: call an 'alter' callback on the control structure
    if (!is_array($control['next'])) {
      $control['next'] = array(
        'ok' => $control['next'],
        'error' => 'STOP',
      );
    }
    foreach (array('description', 'optional', 'unnecessary') as $message_key) {
      if (!empty($control[$message_key])) {
        drush_print($control[$message_key]);
      }
    }
    $control['callback'] = $control['callback'];
    $selection_options = $control['selection-options'];
    $stage_result = TRUE;
    $prompt_user = TRUE;
    if (!$control['callback']) {
      if (empty($control['unnecessary'])) {
        drush_print(dt("Drush cannot do this step; you must perform it manually."));
      }
      $selection_options += array(
        '#skip' => dt("I did the step manually; continue."),
      );
    }
    elseif (!empty($control['required'])) {
      $selection_options['run'] = $control['required'];
    }
    else {
      $selection_options += array(
        'run' => dt("Do it for me."),
        '#manual' => dt("I will do this step manually."),
      );
    }
    if (!empty($control['optional']) || !empty($control['unnecessary'])) {
      $selection_options += array(
        '#skip' => dt("Skip this step."),
      );
    }
    if (($control['callback']) && ($control['straightforward'] || $control['unnecessary']) && $fsm_context['straightforward']) {
      $prompt_user = FALSE;
    }
    if ($prompt_user) {
      $control['selection'] = drush_choice($selection_options, $control['prompt']);
    }
    else {
      $control['selection'] = 'run';
    }
    if (!$control['selection']) {
      return drush_user_abort();
    }
    // TODO: call a 'post selection' callback on the control structure
    if (($control['selection']{0} != '#') && $control['callback']) {
      $stage_result = $control['callback']($fsm_context, $control);
    }
    if ($stage_result === TRUE) {
      $stage_result = 'ok';
    }
    elseif ($stage_result === FALSE) {
      $stage_result = 'error';
    }
    if (array_key_exists($stage_result, $control['next'])) {
      // If $control['next'][$stage_result] is an array, prompt the user
      // and ask which step to do next; otherwise, the value is
      // the next stage to go to.
      $state = $control['next'][$stage_result];
      if (is_array($state)) {
        $next_state_prompt = dt("What would you like to do?");
        if (array_key_exists('#prompt', $state)) {
          $next_state_prompt = $state['#prompt'];
          unset($state['#prompt']);
        }
        $state = drush_choice($state, $next_state_prompt);
      }
    }
    else {
      return drush_set_error('DRUSH_UPGRADE_STATE_MACHINE_ERROR', dt("Cannot continue; no next stage."));
    }
  }
}

function drush_upgrade_check_modules_and_themes(&$fsm_context, $control) {
  $source_version = $fsm_context['source_version'];
  $target_version = $fsm_context['target_version'];
  $target_key = $fsm_context['target_key'];
  $target_alias = $fsm_context['target_alias'];

  drush_print(dt("Gathering information about contrib module upgrade availability..."));
  // Get a list of enabled contrib extensions.
  $values = drush_invoke_process('@self', 'pm-list', array(), array('status'=>'enabled','no-core'=>TRUE), array('integrate' => FALSE, 'override-simulated' => TRUE));
  if ($values['error_status'] != 0) return FALSE;
  $contrib_extensions = array_keys($values['object']);

  // Get a list of enabled core extensions.
  $values = drush_invoke_process('@self', 'pm-list', array(), array('status'=>'enabled','core'=>TRUE), array('integrate' => FALSE, 'override-simulated' => TRUE));
  if ($values['error_status'] != 0) return FALSE;
  $core_extensions = array_keys($values['object']);

  // Make a list of modules that are not available to be enabled in the target
  // version of Drupal, either because they have not been ported yet, or because
  // they have been rolled into core.
  $unavailable_extensions = array();

  // Get the list of modules the user would like to uninstall (if any).
  $uninstall_extensions = drush_get_option('uninstall', '');
  if ($uninstall_extensions == "all") {
    $uninstall_extensions = $contrib_extensions;
  }
  else {
    $uninstall_extensions = explode(',', $uninstall_extensions);
  }

  // WARN:  Tell the user about any special situations that might exist with contrib modules.

  $project_download_list = array();
  $project_info_map = array();
  $extension_info = drush_get_extensions();
  if (!empty($contrib_extensions) && !drush_get_option('core-only')) {
    // Make a list of all of the extensions to download.  We will
    // download everything in the contrib extension list, but we
    // will skip projects that already exist.
    $special_projects = drush_upgrade_project_map($target_version);
    $in_core = array();
    $special_warning = array();
    foreach ($contrib_extensions as $extension_name) {
      drush_print($extension_name);
      // Only check extensions that are NOT in our uninstall list.
      if (!in_array($extension_name, $uninstall_extensions) && array_key_exists($extension_name, $extension_info)) {
        if (empty($extension_info[$extension_name]->info['project'])) {
          drush_log(dt('No project information could be found for the module !module. Enable git_deploy project if you are using Git clones as projects.', array('!module' => $extension_name)), 'warning');
        }
        else {
          $project = $extension_info[$extension_name]->info['project'];
          $project_info_map[$project]['name'] = $extension_info[$extension_name]->info['name'];
          $project_info_map[$project]['version'] = $extension_info[$extension_name]->info['version'];
          // Check our lookup table to see if a project has been renamed.
          if (array_key_exists($project, $special_projects['project-remap'])) {
            $project = $special_projects['project-remap'][$project];
          }
          // If the module has been rolled into core in the next major release of
          // Drupal, then we do not need to download it.  Add it to an array for
          // reporting purposes.
          if ((in_array($project, $special_projects['projects-in-core'])) || (in_array($extension_name, $special_projects['modules-in-core']))) {
            $in_core[$extension_name] = $extension_name;
            // Might some of these need to be enabled?
            $unavailable_extensions[] = $extension_name;
          }
          elseif (($extension_info[$extension_name]->type == 'module') && !is_dir($destination_core . '/sites/all/modules/' . $project) && !is_dir($destination_core . '/' . $destination_conf_path . '/modules/' . $project)) {
            $project_download_list[$project][] = $extension_name;
          }
          // If there is a special warning about a project, then add it
          // to the warning list for later reporting.
          if (array_key_exists($project . ':' . $extension_name, $special_projects['warning'])) {
            $special_warning[$project] = $special_projects['warning'][$project . ':' . $extension_name];
          }
          elseif ((array_key_exists($project, $special_projects['warning'])) && (!array_key_exists($project, $special_warning))) {
            $special_warning[$project] = $special_projects['warning'][$project];
          }
        }
      }
    }

    $rows[] = array(dt('Project'), dt('Title'), dt('Installed'), dt('Available'), dt('Status'));
    // Consider each project from the contrib extensions and check with PM to see if there is
    // a recommended release available to download.  If there is NO release available, then
    // we are not ready to upgrade (but still can, without that project); if there is no RECOMMENDED
    // release, then we might not be ready to upgrade (but still can, with a non-recommended release).
    if (!empty($project_download_list)) {
      $result = drush_invoke_process('@none', 'pm-releases', array_keys($project_download_list), array('default-major' => $target_version), array('integrate' => FALSE, 'override-simulated' => TRUE, 'log' => FALSE));
      $project_releases = $result['object'];
      foreach ($project_download_list as $project => $extension_list) {
        if (!array_key_exists($project, $project_releases) || empty($project_releases[$project]['releases'])) {
          $unavailable_extensions = array_merge($unavailable_extensions, $extension_list);
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
        $title = $project_info_map[$project]['name'];
        $installed_version = $project_info_map[$project]['version'];
        $rows[] = array($project, $title, $installed_version, $available_version, $status);
      }
    }
    drush_print_table($rows);

    // Print out some messages about projects that migrated  to core, or modules that will require special processing.
    if (!empty($in_core)) {
      drush_log(dt('The following contrib modules were enabled in your Drupal site, but are now standard in core: !in_core.  These modules may need to be reconfigured after the upgrade is complete.', array('!in_core' => implode(', ', $in_core))), 'ok');
    }
    foreach ($special_warning as $project => $warning) {
      if ($warning === TRUE) {
        $warning = 'Please see !project_page and !source for more information on how to do this.';
      }
      if ($warning === FALSE) {
        $warning = 'So far there is no indication of when a migration path will be provided.  Check !project_page for updates.';
        $ready_to_upgrade = 'maybe';
      }
      drush_log(dt("You are using the project !project, which requires data migration or other special processing.  $warning", array('!project' => $project, '!project_page' => 'http://drupal.org/project/' . $project, '!source' => $special_projects['source'])), 'warning');
    }
  }
  return TRUE;
}

/**
 * Old version of the code; this logic all needs to be refactored into stages run by the FSM.
 */
function drush_upgrade_site_upgrade_old_version($target_key = NULL) {
  // Presume we are ready to go (n.b. some checks already performed in 'validate')
  $ready_to_upgrade = TRUE;
  $result = TRUE;

  // PREPARE:  Find the target version and determine the contrib projects and enabled modules installed.

  $source_version = drush_drupal_major_version();
  $target_version = $source_version + 1;
  if (isset($target_key)) {
    $target_alias = drush_sitealias_get_record($target_key);
    if (!empty($target_alias)) {
      $destination_core = $target_alias['root'];

      $destination_conf_path = conf_path();
      if (drush_get_option('force-sites-default')) {
        $destination_conf_path = 'sites/default';
      }
    }
  }

  // Get a list of enabled contrib extensions.
  $values = drush_invoke_process('@self', 'pm-list', array(), array('status'=>'enabled','no-core'=>TRUE), array('integrate' => FALSE, 'override-simulated' => TRUE));
  if ($values['error_status'] != 0) return FALSE;
  $contrib_extensions = array_keys($values['object']);

  // Get a list of enabled core extensions.
  $values = drush_invoke_process('@self', 'pm-list', array(), array('status'=>'enabled','core'=>TRUE), array('integrate' => FALSE, 'override-simulated' => TRUE));
  if ($values['error_status'] != 0) return FALSE;
  $core_extensions = array_keys($values['object']);

  // Make a list of modules that are not available to be enabled in the target
  // version of Drupal, either because they have not been ported yet, or because
  // they have been rolled into core.
  $unavailable_extensions = array();

  // Get the list of modules the user would like to uninstall (if any).
  $uninstall_extensions = drush_get_option('uninstall', '');
  if ($uninstall_extensions == "all") {
    $uninstall_extensions = $contrib_extensions;
  }
  else {
    $uninstall_extensions = explode(',', $uninstall_extensions);
  }

  // WARN:  Tell the user about any special situations that might exist with contrib modules.

  $project_download_list = array();
  $project_info_map = array();
  $extension_info = drush_get_extensions();
  if (!empty($contrib_extensions) && !drush_get_option('core-only')) {
    // Make a list of all of the extensions to download.  We will
    // download everything in the contrib extension list, but we
    // will skip projects that already exist.
    $special_projects = drush_upgrade_project_map($target_version);
    $in_core = array();
    $special_warning = array();
    foreach ($contrib_extensions as $extension_name) {
      // Only check extensions that are NOT in our uninstall list.
      if (!in_array($extension_name, $uninstall_extensions) && array_key_exists($extension_name, $extension_info)) {
        if (empty($extension_info[$extension_name]->info['project'])) {
          drush_log(dt('No project information could be found for the module !module. Enable git_deploy project if you are using Git clones as projects.', array('!module' => $extension_name)), 'warning');
        }
        else {
          $project = $extension_info[$extension_name]->info['project'];
          $project_info_map[$project]['name'] = $extension_info[$extension_name]->info['name'];
          $project_info_map[$project]['version'] = $extension_info[$extension_name]->info['version'];
          // Check our lookup table to see if a project has been renamed.
          if (array_key_exists($project, $special_projects['project-remap'])) {
            $project = $special_projects['project-remap'][$project];
          }
          // If the module has been rolled into core in the next major release of
          // Drupal, then we do not need to download it.  Add it to an array for
          // reporting purposes.
          if ((in_array($project, $special_projects['projects-in-core'])) || (in_array($extension_name, $special_projects['modules-in-core']))) {
            $in_core[$extension_name] = $extension_name;
            // Might some of these need to be enabled?
            $unavailable_extensions[] = $extension_name;
          }
          elseif (($extension_info[$extension_name]->type == 'module') && !is_dir($destination_core . '/sites/all/modules/' . $project) && !is_dir($destination_core . '/' . $destination_conf_path . '/modules/' . $project)) {
            $project_download_list[$project][] = $extension_name;
          }
          // If there is a special warning about a project, then add it
          // to the warning list for later reporting.
          if (array_key_exists($project . ':' . $extension_name, $special_projects['warning'])) {
            $special_warning[$project] = $special_projects['warning'][$project . ':' . $extension_name];
          }
          elseif ((array_key_exists($project, $special_projects['warning'])) && (!array_key_exists($project, $special_warning))) {
            $special_warning[$project] = $special_projects['warning'][$project];
          }
        }
      }
    }

    $rows[] = array(dt('Project'), dt('Title'), dt('Installed'), dt('Available'), dt('Status'));
    // Consider each project from the contrib extensions and check with PM to see if there is
    // a recommended release available to download.  If there is NO release available, then
    // we are not ready to upgrade (but still can, without that project); if there is no RECOMMENDED
    // release, then we might not be ready to upgrade (but still can, with a non-recommended release).
    if (!empty($project_download_list)) {
      $result = drush_invoke_process('@none', 'pm-releases', array_keys($project_download_list), array('default-major' => $target_version), array('integrate' => FALSE, 'override-simulated' => TRUE, 'log' => FALSE));
      $project_releases = $result['object'];
      foreach ($project_download_list as $project => $extension_list) {
        if (!array_key_exists($project, $project_releases) || empty($project_releases[$project]['releases'])) {
          $unavailable_extensions = array_merge($unavailable_extensions, $extension_list);
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
        $title = $project_info_map[$project]['name'];
        $installed_version = $project_info_map[$project]['version'];
        $rows[] = array($project, $title, $installed_version, $available_version, $status);
      }
    }
    drush_print_table($rows);

    // Print out some messages about projects that migrated  to core, or modules that will require special processing.
    if (!empty($in_core)) {
      drush_log(dt('The following contrib modules were enabled in your Drupal site, but are now standard in core: !in_core.  These modules may need to be reconfigured after the upgrade is complete.', array('!in_core' => implode(', ', $in_core))), 'ok');
    }
    foreach ($special_warning as $project => $warning) {
      if ($warning === TRUE) {
        $warning = 'Please see !project_page and !source for more information on how to do this.';
      }
      if ($warning === FALSE) {
        $warning = 'So far there is no indication of when a migration path will be provided.  Check !project_page for updates.';
        $ready_to_upgrade = 'maybe';
      }
      drush_log(dt("You are using the project !project, which requires data migration or other special processing.  $warning", array('!project' => $project, '!project_page' => 'http://drupal.org/project/' . $project, '!source' => $special_projects['source'])), 'warning');
    }
  }

  // VALIDATE: did the user provide legal and usable parameters?

  if (drush_upgrade_site_upgrade_check_parameters($target_key) === FALSE) {
    return FALSE;
  }

  // CONFIRM:  Ask the user before overwriting an exsiting site, and ask if an upgrade is really decided if the site is not ready yet.

  // Summarize whether or not there is a good chance that the site can be upgraded.
  if ($ready_to_upgrade !== TRUE) {
    drush_log(dt("Based on the contrib modules enabled in this site, it is possible that the site-upgrade command might fail.  See warnings above."), (($ready_to_upgrade === FALSE) ? 'warning' : 'notice'));
  }
  // Check to see what we should do if the target Drupal folder already exists.
  $options = array(
    'replace' => dt("Delete the existing site and start over."),
    'reuse' => dt("Re-use the existing code, re-import the database from the source site and run updatedb again."),
  );
  $selection = NULL;
  foreach ($options as $option => $msg) {
    if (drush_get_option($option, FALSE)) {
      $selection = $option;
    }
  }
  if (!isset($selection) && (file_exists($destination_core))) {
    $selection = drush_choice($options, dt("Drupal site already exists at !root.  Would you like to:", array('!root' => $destination_core)));
    if (!$selection) {
      return drush_user_abort();
    }
  }
  elseif($ready_to_upgrade !== TRUE) {
    if (!drush_confirm('Would you like to continue anyway?')) {
      return drush_user_abort();
    }
  }
  
  if ($selection == 'reuse') {
    return drush_set_error('DRUSH_SITE_UPGRADE_REUSE_UNSUPPORTED', dt("Unfortunately, the reuse option does not work correctly. You must upgrade manually."));
  }

  // User has already been prompted; skip further confirms.
  drush_set_context('DRUSH_AFFIRMATIVE', TRUE);

  // We also need to set 'invoke', so that calls to drush_invoke do not
  // complain about cli options passed to site-upgrade that are not appropriate
  // for our subcommands (e.g. sql-sync and pm-download) do not throw errors.
  drush_set_option('invoke', TRUE);

  // We need to know where our destination settings file is regardless of which
  // code path we take; therefore, we will precompute it here.

  $settings_destination = $destination_core . '/' . $destination_conf_path . '/settings.php';

  // STEP 1:  Download the next major version of Drupal.

  if (($selection == 'replace') || (!is_dir($destination_core))) {
    drush_upgrade_download_drupal($target_version, $destination_core);
    if (drush_get_error()) return FALSE; // Early exit if we see an error.
    drush_upgrade_copy_settings($target_alias, $settings_destination);
  }
  else {
    // Move sites/all/modules and $conf_path()/modules out of the way
    // so that updatedb can be run on core only.
    if (_drush_upgrade_preserve_modules($destination_core) === FALSE) {
      return FALSE;
    }
  }

  // Copy source database to target database. The source DB is not changed.
  // Always set 'common' at minimum. Sites that want other can create other key in drushrc.php.
  if (!drush_get_option('structure-tables-key')) {
    drush_set_option('structure-tables-key', 'common');
  }
  // Empty or create the target database as needed.
  drush_sql_empty_db(drush_sitealias_get_db_spec($target_alias));
  // We still have DRUSH_AFFIRMATIVE set from above, so this will not prompt.
  drush_invoke('sql-sync', array('@self', $target_key));
  if (drush_get_error()) return FALSE; // Early exit if we see an error.

  if (!empty($contrib_extensions)) {
    $target_alias_databases = sitealias_get_databases_from_record($target_alias);
    $modify_site_conf_path = NULL;

    // Make an alias record that uses the CODE from @self and the DATABASE from $target.
    // Since we just did an sql-sync from @self to @target, we can use this hybrid specification
    // to do manipulations on the target database before runing updatedb.  In brief, we are going
    // to disable all contrib modules to prevent problems with updatedb.
    $modify_site = array (
      'root' => DRUPAL_ROOT,
      'uri' => $target_alias_databases['default']['default']['database'],
    );

    if (!drush_get_context('DRUSH_SIMULATE')) {
      // In theory, if the sql-sync worked, this should never be empty.
      if (empty($modify_site['uri'])) {
        return drush_set_error('DRUSH_UPGRADE_DATABASE_SPEC_UNKNOWN', dt('Failed to look up database spec for @target', array('@target' => $target_key)));
      }
      $modify_site_conf_path = dirname(conf_path()) . '/' . $modify_site['uri'];
      $modify_site_settings = $modify_site_conf_path . '/settings.php';
      drush_log('Set up a transient site by copying ' . $settings_destination . ' to ' . $modify_site_settings, 'debug');
      if (!file_exists($modify_site_settings)) {
        if ((drush_mkdir($modify_site_conf_path, TRUE) === FALSE) || drush_op('copy', $settings_destination, $modify_site_settings) !== TRUE) {
          return drush_set_error('DRUSH_UPGRADE_COULD_NOT_DISABLE', dt("Could not create a temporary multisite "));
        }
      }
    }
    $result = (drush_invoke_process($modify_site, 'site-upgrade-prepare', $contrib_extensions, array('uninstall' => implode(',', $uninstall_extensions), 'yes' => TRUE), array('interactive' => TRUE)) == 0);

    // Delete the temporary site now that we're done with it.
    if (isset($modify_site_conf_path)) {
      drush_delete_dir($modify_site_conf_path);
    }
    if ($result === FALSE) return FALSE;
  }

  // STEP 2:  Call updatedb for Drupal core.

  // Run update.php in a subshell. It is run on @target site whereas this request was on @self.
  drush_log(dt('About to perform updatedb for Drupal core on !target', array('!target' => $target_key)), 'ok');
  // When we call drush_invoke_process in #interactive mode, the result code comes from drush_op_system, where 0 == success.
  $result = drush_invoke_process($target_alias, 'updatedb', array(), array('yes' => TRUE), array('interactive' => TRUE)) == 0;
  if ($result === FALSE) {
    return drush_set_error('DRUSH_DRUPAL_CORE_UPGRADE_FAILED', dt("The call to updatedb failed for Drupal core.  This may be caused by a contrib module that is not yet ready for upgrade.  Try running site-upgrade again with '--uninstall={module list}' to remove all contrib extensions prior to upgrade.  Add modules back in until the problematic one is isolated.  Please report problems in the issue queue of the module that is causing problems."));
  }
  drush_log(dt('updatedb complete for Drupal core'), 'ok');

  // If we moved our modules out of the way, bring them back now.
  _drush_upgrade_restore_preserved_modules();

  // STEP 3: Download and re-enable the contrib modules.

  if (!empty($contrib_extensions) && !drush_get_option('core-only')) {
    $options = array();
    $backend_options = array('interactive' => TRUE);
    if (!empty($project_download_list)) {
      $projects = implode(',', array_keys($project_download_list));
      $options['projects'] = $projects;
    }
    // If a module changed name, then rename it prior to calling pm-enable.
    foreach ($contrib_extensions as $extension_name) {
      if (array_key_exists($extension_name, $special_projects['module-remap'])) {
        $unavailable_extensions[] = $extension_name;
        $contrib_extensions[] = $special_projects['module-remap'][$extension_name];
      }
    }

    // Redispatch to site-upgrade-modules command, so that we will be
    // bootstrapped to the target site.
    $result = (drush_invoke_process($target_alias, 'site-upgrade-modules', array_merge($core_extensions, array_diff($contrib_extensions, $unavailable_extensions, $uninstall_extensions)), $options, $backend_options) == 0);
  }

  return $result;
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
      'project-remap' => array(),
      'module-remap' => array(
        'content' => 'cck',
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
 * Prepare to upgrade; the first step is to disable all contrib modules.
 */
function drush_upgrade_site_upgrade_prepare() {
  $contrib_extensions = func_get_args();
  $uninstall_extensions = explode(',', drush_get_option('uninstall', ''));

  // Set theme back to garland per Upgrade.txt.
  variable_set('theme_default', 'garland');

  // http://drupal.org/node/724102 recommends using "seven" as your admin theme.  Don't switch it to garland if it is already seven.
  $admin_theme = variable_get('admin_theme', NULL);
  if ($admin_theme != "seven") {
    variable_set('admin_theme', 'garland');
  }
  else {
    drush_log(dt("Admin theme is already set to 'seven'."), 'ok');
  }

  // Disable all contrib modules per Upgrade.txt.
  drush_log(dt("Disabling !list", array('!list' => implode(", ", $contrib_extensions))), 'ok');
  drush_invoke('pm-disable', $contrib_extensions);
  if (drush_get_error()) return FALSE; // Early exit if we see an error.

  // Uninstall any modules specified via the --uninstall flag.
  if (!empty($uninstall_extensions)) {
    drush_log(dt("Uninstalling !list", array('!list' => implode(", ", $uninstall_extensions))), 'ok');
    drush_invoke('pm-uninstall', $uninstall_extensions);
    if (drush_get_error()) return FALSE; // Early exit if we see an error.
  }
}

/**
 * Upgrade all of the contrib modules of the site being upgraded.
 *
 * This runs bootstrapped to the TARGET site, after the new version
 * of Drupal has been downloaded, and after updatedb has been run
 * for Drupal core.
 */
function drush_upgrade_site_upgrade_modules() {
  $extensions_to_enable = func_get_args();
  $project_download_list = drush_get_option('projects', '');

  if (!empty($project_download_list)) {
    // Download our contrib extensions.
    drush_log(dt('Download projects: !projects', array('!projects' => $project_download_list)), 'ok');
    drush_set_option('destination', NULL);
    // User has already been prompted; if there is no recommended release,
    // we will just take the most recent.
    drush_set_option('choice', '1');
    drush_invoke('pm-download', $project_download_list);
  }

  // Run updatedb to update all of the contrib extensions.
  drush_log(dt('About to perform updatedb for extensions'), 'ok');
  $result = drush_invoke_process('@self', 'updatedb', array(), array('yes' => TRUE), array('interactive' => TRUE));
  if ($result === FALSE) {
    return drush_set_error('DRUSH_DRUPAL_CONTRIB_UPGRADE_FAILED', dt("The call to updatedb failed for the enabled contrib modules.  Try running site-upgrade again with '--uninstall={module list}' to remove all contrib extensions prior to upgrade.  Add modules back in until the problematic one is isolated.  Please report problems in the issue queue of the module that is causing problems."));
  }
  drush_log(dt('updatedb complete for extensions'), 'ok');

  // Finally, enable the modules that site-upgrade previously disabled.
  // We will set the option --resolve-dependencies to pick up new modules
  // that may now be required; for example, views-7.x picked up a dependency
  // on ctools that views-6.x did not have.  We also set DRUSH_AFFIRMATIVE,
  // so everything from here on out will be processed with --yes.
  drush_set_option('resolve-dependencies', TRUE);
  drush_set_context('DRUSH_AFFIRMATIVE', TRUE);
  $result = drush_invoke('pm-enable', $extensions_to_enable);

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
    $databases = drush_sitealias_convert_db_from_db_url($new_url);
  }
  else {
    $databases = $GLOBALS['databases'];
    $databases['default']['default']['database'] = $target_alias_databases['default']['default']['database'];
  }
  $append .= "\n" . '$databases = ' . var_export($databases, TRUE) . ';';
  // Caching the database record in the alias record allows sql-sync to work
  // before updatedb is called. updatedb is what converts from a db_url to a
  // DBTNG array; this conversion is required by sql-sync.
  drush_sitealias_cache_db_settings($target_alias, $databases);

  // Also append the new configuration options to the end of settings.php.
  drush_op('file_put_contents', $settings_destination, $append, FILE_APPEND);
}

/**
 * Rollback function: restore our modules if updatedb fails.
 */
function drush_upgrade_site_upgrade_rollback($target_key) {
  _drush_upgrade_restore_preserved_modules();
}

/**
 * Preserve existing modules.  Move them out of the way prior
 * to updatedb of Drupal core.  We will move them back afterwards.
 */
function _drush_upgrade_preserve_modules($destination_core) {
  $modules_preserve['root'] = $destination_core;
  $modules_preserve['list'] = array();

  $moduledir_list = array(
    'sites_all_modules' => $destination_core . '/sites/all/modules',
    'sites_conf_path_modules' => $destination_core . conf_path() . '/modules',
  );

  foreach ($moduledir_list as $moduledir_name => $moduledir) {
    if (is_dir($moduledir)) {
      $preserved_moduledir = drush_tempnam($moduledir_name, dirname($moduledir));
      $result = drush_move_dir($moduledir, $preserved_moduledir, TRUE);
      if ($result) {
        $modules_preserve['list'][$moduledir] = $preserved_moduledir;
        drush_log(dt('Move !src to !dest prior to updatedb on Drupal core.', array('!src' => $moduledir, '!dest' => $preserved_moduledir)), 'ok');
      }
      else {
        return drush_set_error('DRUSH_MODULE_PRESERVE_FAILED', dt('Failed to move !src to !dest.', array('!src' => $moduledir, '!dest' => $preserved_moduledir)));
      }
    }
  }

  drush_set_context('DRUSH_MODULES_PRESERVE', $modules_preserve);
  return TRUE;
}

/**
 * Restore modules that were preserved by _drush_upgrade_preserve_modules.
 */
function _drush_upgrade_restore_preserved_modules() {
  $modules_preserve = drush_get_context('DRUSH_MODULES_PRESERVE', array());

  if (!empty($modules_preserve) && array_key_exists('list', $modules_preserve)) {
    foreach ($modules_preserve['list'] as $moduledir => $preserved_moduledir) {
      drush_move_dir($preserved_moduledir, $moduledir, TRUE);
    }
  }
  drush_set_context('DRUSH_MODULES_PRESERVE', array());
}
