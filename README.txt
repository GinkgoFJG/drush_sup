Overview of Upgrade Procedure:
==============================

1. The Drush site-upgrade command conforms very closely to the module 
   upgrade procedure described at http://drupal.org/node/948216 
   (step 15 of UPGRADE.txt). Modules are enabled and updated in order,
   one at a time.  No module is be updated until all of the modules it 
   depends on have been successfully updated.

2. The site-upgrade command is implemented as a finite state machine.
   Each state clearly describes what it is doing, and the states follow the 
   stages of UPGRADE.txt nearly exactly.  One important difference is
   that UPGRADE.txt instructs you to upgrade a site in place, whereas
   Drush will copy the source site to a new site during the upgrade.
   Because of this, some steps happen in a slightly different order than
   described in UPGRADE.txt.  All differences are explained in the
   explanitory text of the upgrade stages.
   
3. If you run with the flag --confirm-all, then every stage will prompt 
   for what action to take.  If a situation that Drush cannot handle
   is encountered, it is possible to pause the upgrade (select "Cancel"
   at any prompt), fix the problem manually, and then continue the upgrade
   again from where it last left off.

4. Any step of the upgrade process that successfully completes an updatedb 
   will automatically save a backup copy of the partially upgraded site's 
   code and database using the archive-dump command. Two backups are maintained
   during the lifetime of the upgrade command--one after Drupal core is
   updated, and one after the most recent successful module update.

5. If the source site is changed, you may restart the site-upgrade from the
   beginning, and copy and update the modified database.  You may opt to keep
   any code modifications you made during the previous upgrade.


Upgrade Problems and Workarounds:
=================================

If you use features, you will want to run `drush fia` on your source site
before beginning.  See: http://drupal.org/node/1014522#comment-5478110

Error updating uuid module:  Apply the patch at http://drupal.org/node/1469942
to fix.  If you see error messages after updating uuid, you must restore to
the latest working backup and then apply the aforementioned patch to the uuid module

Token module error on enable:  if you are using the token module, you will find
that an error is thrown during the enable step.  If you see error messages after
updating the token module, you may simply run updatedb on the target site manually,
and then continue with the upgrade.  Optionally, apply the cache_clear_all patch at 
http://drupal.org/node/1477932 to Drupal core before enabling the token module.


Wishlist Features:
==================

Tell Drush Where to get Module Files (#5 above)
------------------------------------
It should be possible to tell Drush to get modules from some other
source than pm-download, e.g. from a local git repository, from
a cache folder, etc.  Maybe we should even allow the user to specify
a makefile that will pull down specific versions of certain modules
using any selection feature available in Drush make. Anything other than
"from a folder" is probably a "wishlist" feature.

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


Things I did Manually Durring and After My Site Upgrade
=======================================================

drush @wk.dev sup @wk.d7dev # run through 'Update core'
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

