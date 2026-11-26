/**
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var reminders = require('./lib/reminders');
var diagnostics = require('./agent/diagnostics');

// Total send attempts per reminder before giving up on it (was: retry the
// same reminder forever, which could wedge the whole list transfer).
var MAX_SEND_ATTEMPTS = 3;

function handleReminderMessage(data) {
  if (data.REMINDER_LIST_REQUEST) {
    var allReminders = reminders.getAllReminders();

    // Convert from storage format to menu format
    var menuReminders = allReminders.map(function(r) {
      return {
        id: r.id,
        text: r.what,
        time: new Date(r.time)
      };
    });

    diagnostics.record('reminder_list', 'list request: sending count=' + menuReminders.length);

    // First send the count
    Pebble.sendAppMessage({
      'REMINDER_COUNT': menuReminders.length
    });

    // Then send each reminder
    function sendNextReminder(index, attempt) {
      if (index >= menuReminders.length) {
        diagnostics.record('reminder_list', 'list transfer complete: ' + menuReminders.length + ' reminder(s)');
        return;
      }

      var reminder = menuReminders[index];
      var preview = 'time=' + reminder.time.toISOString() + ' text="' + String(reminder.text).substring(0, 40) + '"';
      Pebble.sendAppMessage({
        'REMINDER_TEXT': reminder.text,
        'REMINDER_ID': reminder.id,
        'REMINDER_TIME': Math.floor(reminder.time.getTime() / 1000)
      }, function() {
        // On success, send next reminder
        diagnostics.record('reminder_list', 'sent ' + (index + 1) + '/' + menuReminders.length + ': ' + preview);
        sendNextReminder(index + 1, 1);
      }, function() {
        // On failure, retry this reminder a bounded number of times
        console.error('Failed to send reminder:', reminder);
        if (attempt >= MAX_SEND_ATTEMPTS) {
          diagnostics.record('reminder_list', 'FAILED after ' + MAX_SEND_ATTEMPTS + ' attempts, skipping: ' + preview);
          sendNextReminder(index + 1, 1);
        } else {
          diagnostics.record('reminder_list', 'send failed (attempt ' + attempt + '/' + MAX_SEND_ATTEMPTS + '), retrying: ' + preview);
          sendNextReminder(index, attempt + 1);
        }
      });
    }

    // Start sending reminders
    sendNextReminder(0, 1);
    return true;
  } else if (data.REMINDER_DELETE) {
    var id = data.REMINDER_DELETE;
    diagnostics.record('reminder_delete', 'delete requested id=' + id);
    try {
      reminders.deleteReminder(id);
      diagnostics.record('reminder_delete', 'delete done id=' + id);
    } catch (err) {
      console.error('Failed to delete reminder:', err);
      diagnostics.record('reminder_delete', 'delete failed id=' + id + ': ' + err.message);
    }
    return true;
  }
  return false;
}

module.exports = {
  handleReminderMessage: handleReminderMessage
}; 
