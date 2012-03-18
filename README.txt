DESCRIPTION
===========

The Drush site-upgrade command automates the process of upgrading a Drupal site
per the instructions in UPGRADE.txt.  There are two primary benefits of using
Drush site-upgrade over the manual process:

 1. Automation of the tedious process required to upgrade contrib modules
    saves on time and reduces the odds of a failed upgrade due to a mis-step.
 
 2. Advice is given on replacement modules to use in instances where a module in
    use on the old site has no upgrade path.

The site-upgrade command can be stopped in the middle, resumed, and ran as many
times as needed to perform a successful upgrade.

Note that you will still need to port the CODE for your custom theme and modules,
if any are in use.


REQUIREMENTS
============

* Drush-5RC5 or later required.

* Currently, only Drupal-6 to Drupal-7 upgrades are supported.


INSTALLATION
============

Follow the git instructions at http://drupal.org/node/1408380/git-instructions/master
to download the latest version of the Drush site-upgrade command.  See the Drush
README.txt for locations where Drush extensions may be placed.

FURURE: `drush dl drush_sup` will be sufficient once there is a drush_sup release.


USAGE
=====

 1. Read the UPGRADE.txt document in Drupal core of the version of Drupal you are
    upgrading to, and understand the process.  Also carefully study the module
    upgrade procedure, available at the URL specified in UPGRADE.txt.  Drush Site
    Upgrade follows the process described in UPGRADE.txt very closely; however,
    one very important difference is that UPGRADE.txt instructs you to upgrade
    your Drupal site in place, whereas `drush site-upgrade` will upgrade to
    a separate copy of your site, leaving the orginal unmodified.

 2. Use `drush pm-update` to do a minor version update of your source site to the
    most recent available version of core and contrib.  All of your code must be
    up to date before you begin the major version upgrade.

 3. Run `drush sup` once on your source site and review the upgrade readiness report.

        $ cd /path/to/drupal
        $ drush site-upgrade

     - OR -

        $ drush @d6 site-upgrade

    The site-upgrade will examine the modules that you have installed,
    and will report on the availability of Drupal 7 versions of the same
    modules.  For modules which have no version available in Drupal 7,
    Drush Site Upgrade will offer to substitute alternate projects that
    may perform similar functions.  Sometimes there is more than one option
    available, in which case Drush will pick the one that it thinks is best,
    but will offer you the chance to select a different one if you wish.
    You may see a message similar to this:

       cck: The module nodereference in this project has no D7 version 
       available, but there are multiple alternative projects that may  
       be used instead. Drush picked references:node_reference, but 
       entityreference also available.  Run again with --preferred to
       select a different preference.

    If you would rather use entityreference than references, then run
    drush site-upgrade again with the --preferred option:

        $ drush @d6 site-upgrade --preferred=entityreference

    If you have multiple alternative preferences to separate, include them
    all in a comma-separated list.  Read the project page of each
    alternative to choose the module that seems to best meet your needs.

    Sometimes, there may be preliminary steps are required before beginning 
    the upgrade; if so, you will be advised of what needs to be done.
    For example, if you are using features, you should import all of your
    features into the database using the `drush features-import-all`
    command.  Instructions on doing this is given in the upgrade readiness
    report.

    You may decide at this point that you cannot upgrade your site, because 
    needed modules or themes are not ready for Drupal 7.  If you would
    like to attempt an upgrade, procede to the next step.  Unavailable modules will
    be skipped.  You can always re-run the upgrade again later if you wish.

 4. Prepare a site alias to describe where drush should put the upgraded
    site:
    
        $aliases['d7upgrade'] = array(
          'root' => '/path/to/upgradeddrupal',
          'uri' => 'mydrupalsite.org',
        );
    
    Place this alias in any file where aliases may be stored.  See the
    Drush documentation for more information.

 5. Choose the amount of prompting that you would like, and run `drush
    site-upgrade` again.  The upgrade process involves many steps, and
    produces a lot of output.  How much prompting you would like will
    vary depending on how confident you are about the upgrade process.
    This can be controlled by the various prompting options:
    
       --confirm-all   
       
       This option is best for your first run through Drush Site Upgrade.
       It will prompt at every stage.  Drush will tell you what it is
       going to do before it does it.  This will give you the opportunity
       to do some investigation in another terminal; you can even opt to
       do the operation manually, and tell Drush to continue with the upgrade.
       
       --confirm-all --skip-optional
       
       The --skip-optional flag is only valid in --confirm-all mode, as
       skip optional is otherwise the default.  There are a number of steps
       in UPGRADE.txt that are only necessary when you are doing an upgrade
       in place.  For example, UPGRADE.txt instructs you to take the source
       site offline before beginning the upgrade.  Since the Site Upgrade
       command works on a copy of your site, you may leave the source site
       online while you work if you wish.  The --skip-optional step will
       cause Drush to skip past this step and all similar steps automatically.
       
       [default]
       
       If none of the prompting options are specified, then
       Drush Site Upgrade will automatically do all of the stages that
       considered "straightforward", and will prompt you only for stages
       where a decision typically needs to be made.  It is recommended to
       run through an upgrade with more prompting at least once before
       reverting to this mode, although it is safe enough to run with just
       the minimal prompting provided by the default mode.
       
       --auto
       
       In fully-automatic mode, Drush Site Upgrade will always choose the
       first (most reasonable) option at all stages, and will prompt you
       only in instances where there is no reasonable default.

 6. Run the Drush site-upgrade command with your selected target alias
    and prompting level:
    
       $ drush @d6 site-upgrade --confirm-all @d7upgrade

 7. Step through the stages of the upgrade process, making note of any
    problems or error messages encountered along the way.  If you stop the
    upgrade in the middle, either by selecting the [Cancel] option or
    due to some error, you may re-run the Drush site-upgrade command again
    to resume the upgrade.  Drush will prompt you for your desired action:
    
     * Resume where you left off:
       
       The last stage executed will be ran again.  If you have a failure
       that can be resolved manually, you can just pick up the upgrade
       from where you last left off.
       
     * Begin again from the start, re-using the same code:
       
       If the target site already exists, the site-upgrade command will
       allow you to run the upgrade again, using the code at the target
       location.  This will allow you to manage code changes required
       during your upgrade; just compose the code you want to use at
       the target site, perhaps via `drush dl` of specific versions, by
       pulling from git, or by using  a Drush Make file, and run site-upgrade 
       with this option.
     
     * Begin again from the start with fresh code
     
       You may also start over from the beginning, once again pulling
       down all code needed via `drush dl` automatically.
     
     * Resume from a backup point
     
       Drush site-upgrade automatically backs up your target site every
       time it runs the updatedb command.  Two backups are retained: the
       one that was taken after Drupal core was upgraded, and the one
       that was taken after the most recent contrib module was successfully
       upgraded.  You may resume from either of these points at any time.

 8. Review the site upgrade report at the end of the upgrade process.
    If all went well, all lines in the report will be labeled 'OK'.  If
    there are any problems reported, repair them manually.
 
 9. Use the content_migrate module to migrate your cck data to Drupal 7
    format.  The site-upgrade command automatically enables content_migrate,
    so you only need to visit the migration page and step through your data types.
  
Once the site-upgrade has completed, if there were no problems, then your
site should be nominally functional, and all of your data should be in place.
There will still be work left to do, though; review all of your configuration
settings, and insure that everything is correct. Some settings may be dropped
during upgrade.  Fine-tuning of your theme may also be necessary.


TODO
====

Tell Drush Where to get Module Files
------------------------------------
It should be possible to tell Drush to get modules from some other
source than pm-download, e.g. from a local git repository, from
a cache folder, etc.  Maybe we should even allow the user to specify
a makefile that will pull down specific versions of certain modules
using any selection feature available in Drush make.

There is an easy workaround for this as it is.  All of the modules
are pre-downloaded to the project cache folder, so the user can
just drop patches and custom versions there.

Error checking on updatedb
--------------------------
If pm-enable fails, the module is added to a "problems" list and
processing on it until later.  The same should be done when a
module's updatedb fails. To recover from this, though, would require
restoring the site from the last backup point.

Updating core pauses for confirmation in updatedb
-------------------------------------------------
--yes should be specified, at least in 'auto' mode.
Perhaps the prompt should only appear in --confirm-all mode.

Refine Handling of Pre-upgrade Warning Messages
-----------------------------------------------
Right now, if there are warnings in the pre-upgrade messages, sup
says "It is possible that the upgrade might fail".  Warning messages
should be classified as 'post-processing needed', 'might fail',
and so on, so this message is not displayed unless failure is actually
possible.  The 'post-processing needed' messages should be repeated
once the upgrade is finished.

Test if contrib modules need updates
------------------------------------
It would be helpful if we could sort out which contrib modules
have update functions that need to run, and which do not. An
indication of which modules required no updates could be given
on the module-selection menu.

Show progress so far
--------------------
In step 15, when displaying the list of modules to be upgraded,
also show a list of all contrib modules that have been successfully
enabled and updated.

Simulated mode
--------------
We could add a "simulate" menu item to every stage with a callback.
If selected, Drush could set SIMULATED, run the selected step, and
then return to prompt the user again.  We could also support a
--simulate-first cli option that would automatically simulate every
step before prompting the user.

Automatic code upgrades
-----------------------
We could use the coder module to do a code upgrade of modules that
do not have upgraded versions available yet.

Upgrade In Place
----------------
Should it be possible to do an upgrade "in place", overwriting the
source site?  While this is not the best way to do an upgrade, it
might be of some benefit to users who wanted to follow the UPGRADE.txt
instructions exactly (e.g. to continue it manually partway through).

Support multiple versions of Drupal
-----------------------------------
Currently, only Drupal 6 to Drupal 7 upgrades are supported.  It
would be possible to make a new FSM table to support Drupal 7 to
Drupal 8 upgrades. (Note: it looks like the UPGRADE.txt for Drupal 8
is substantially similar to the Drupal 7 upgrade process, so the Drupal 8
upgrade support should be limited to providing an appropriate
upgrade project map.)

Set "Seven" as the Admin Theme
------------------------------
Cli option --seven to set the admin theme?  Code will already do this if "Seven"
is your admin theme on D6.

Upgrade Completion Report
-------------------------
We should cache all of the advice given by site-upgrade prior to the upgrade,
plus all of the log messages from updatedb (except for the lines that say only
"Performing xxxx_xxxx_####"), and then provide a separate command
`drush @target upgrade-report` that will dump this info out, so that upgraders
can review progress during and after the upgrade process.  We should save
the progress file after every stage, not just on abort / error.

Automatic detection of modified .htaccess / robots.txt
------------------------------------------------------
If `hacked` is available, or if core was checked out via git from
drupal.org, we could generate a patch file with local modifications
to .htaccess and robots.txt.  Once we had a patch file, we could at least
try to apply it on the upgraded system.  If the modifications consisted only
of appended info to the end of the file, this might be reliable enough. (?)
Need to test; I usually do not modify these files.

Automatic Remediation of Status Report Problems
-----------------------------------------------
We could provide a separate command with a separate FSM state table to step
through the entries from the `core-requirements` (`status-report`) report.
Well-known problems (File System not writable, Rebuild Permissions, etc.)
could be detected and the user could be prompted to choose the action to take.

Note that sup already attempts to clean up prior to running the status
report, to minimize the number of problems that might be reported.

Repair Toolbar Menu
-------------------
See http://drupal.org/node/991778.  We could either have the user reset the
"Administrator" menu item back to the Navigation menu, or delete the system
menus per the workaround in the aforementioned issue (and clear the cache)
after the upgrade completes.


EXAMPLE
=======

Briefly:

drush @wk.dev sup @wk.d7dev # run through 'Update core', then cancel
cd /home/ga/.drush/cache/@self-d6-to-wk.d7dev-d7/project_cache/
patch -Np1 < ~/tmp/patches/uuid-update-install-uuid-fields-13.patch
cdd @wk.d7dev
patch -Np1 < ~/tmp/patches/drupal-7-cache-clear-all.patch
drush @wk.dev sup @wk.d7dev --auto
drush @wk.d7dev en seven
drush @wk.d7dev vset admin_theme 'seven'
cp -R /srv/www/dev.westkingdom.org/sites/all/themes/wk_zen2/css/wk_img/ /srv/www/d7.westkingdom.org/sites/default/files/
drush @wk.d7dev en toolbar
drush @wk.d7dev en shortcut
drush @wk.d7dev en contextual
drush @wk.d7dev sqlq "DELETE FROM drupal_menu_links WHERE module = 'system';"
drush @wk.d7dev cc all


CREDITS
=======

* Based on Moshe Weitzman's original site-upgrade code originally located in Drush core.
* Redesigned and maintained by greg.1.anderson.
