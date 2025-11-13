const CONFIG_HTML = `
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Totper Settings</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 0; padding: 16px; background: #121212; color: #f5f5f5; }
    h1 { font-size: 22px; margin: 0 0 12px; }
    .entry { background: #1f1f1f; border-radius: 12px; padding: 12px; margin-bottom: 12px; }
    .entry label { display: block; font-size: 13px; margin-bottom: 4px; color: #9e9e9e; }
    .entry input { width: 100%; padding: 8px; border-radius: 8px; border: none; margin-bottom: 8px; font-size: 15px; box-sizing: border-box; background: #2c2c2c; color: #f5f5f5; }
    .entry .inline { display: flex; gap: 8px; }
    .entry .inline input { flex: 1; }
    button { border: none; border-radius: 999px; padding: 12px 18px; font-size: 15px; cursor: pointer; margin: 6px 4px 0 0; }
    button.primary { background: #4caf50; color: white; }
    button.secondary { background: #424242; color: #f5f5f5; }
    button.danger { background: #f44336; color: white; }
    button.disabled { opacity: 0.45; cursor: not-allowed; }
    .actions { position: sticky; bottom: 0; display: flex; flex-wrap: wrap; gap: 8px; padding-top: 12px; background: rgba(18,18,18,0.95); }
    #scanner { position: fixed; inset: 0; background: rgba(0,0,0,0.92); display: none; align-items: center; justify-content: center; flex-direction: column; padding: 16px; }
    #scanner video { width: 100%; max-width: 360px; border-radius: 16px; background: #000; }
    #scanner-status { margin-top: 12px; font-size: 14px; color: #b0bec5; text-align: center; }
    #scanner button { margin-top: 16px; }
    .hint { font-size: 13px; color: #b0bec5; margin-bottom: 16px; }
    a { color: #4caf50; }
  </style>
  <h1>Totper</h1>
  <p class="hint">Enter secrets manually or paste otpauth URLs. Default period/digits: 30 / 6.</p>
  <div id="entries"></div>
  <div class="actions">
    <button id="test-log" class="secondary">Test Log</button>
    <button id="add-entry" class="secondary">Add Entry</button>
    <button id="save" class="primary">Save</button>
    <button id="reset" class="danger">Clear All</button>
  </div>

  <script>
  (function() {
    console.log('[Totper] Script started');
    var initialData = decodeURIComponent('__INITIAL_DATA__');
    var entries = [];
    console.log('[Totper] Initial data length:', initialData ? initialData.length : 0);
    try {
      if (initialData) {
        var parsed = JSON.parse(initialData);
        if (Array.isArray(parsed)) {
          entries = parsed;
        }
      }
    } catch (err) {
      console.log('Failed to parse initial data', err);
    }

    var entriesRoot = document.getElementById('entries');
    var pasteButton = document.getElementById('paste-entry');

    function createEntryView(entry, index) {
      var container = document.createElement('div');
      container.className = 'entry';
      container.dataset.index = index;

      container.innerHTML = [
        '<label>Label</label>',
        '<input type="text" name="label" placeholder="e.g. Google" value="' + (entry.label || '') + '" maxlength="32" required>',
        '<label>Account Name (optional)</label>',
        '<input type="text" name="account_name" placeholder="e.g. user@gmail.com" value="' + (entry.account_name || '') + '" maxlength="32">',
        '<label>Secret (Base32)</label>',
        '<input type="text" name="secret" placeholder="JBSWY3DPEHPK3PXP" value="' + (entry.secret || '') + '" maxlength="64" required>',
        '<div class="inline">',
          '<div>',
            '<label>Period (s)</label>',
            '<input type="number" name="period" min="1" max="120" value="' + (entry.period || 30) + '">',
          '</div>',
          '<div>',
            '<label>Digits</label>',
            '<input type="number" name="digits" min="6" max="8" value="' + (entry.digits || 6) + '">',
          '</div>',
        '</div>',
        '<button type="button" class="danger remove-entry">Remove</button>'
      ].join('');

      container.querySelector('.remove-entry').addEventListener('click', function() {
        entries.splice(index, 1);
        renderEntries();
      });
      return container;
    }

    function renderEntries() {
      entriesRoot.innerHTML = '';
      if (!entries.length) {
        var empty = document.createElement('p');
        empty.className = 'hint';
        empty.textContent = 'No entries yet.';
        entriesRoot.appendChild(empty);
      } else {
        entries.forEach(function(entry, idx) {
          entriesRoot.appendChild(createEntryView(entry, idx));
        });
      }
    }

    function readEntriesFromDom() {
      var nodes = entriesRoot.querySelectorAll('.entry');
      var result = [];
      Array.prototype.forEach.call(nodes, function(node) {
        var label = node.querySelector('input[name="label"]').value.trim();
        var account_name = node.querySelector('input[name="account_name"]').value.trim();
        var secret = node.querySelector('input[name="secret"]').value.replace(/\\s+/g, '').toUpperCase();
        var period = parseInt(node.querySelector('input[name="period"]').value, 10);
        var digits = parseInt(node.querySelector('input[name="digits"]').value, 10);
        if (!label || !secret) {
          return;
        }
        if (!period || period < 1 || period > 120) {
          period = 30;
        }
        if (!digits || digits < 6 || digits > 8) {
          digits = 6;
        }
        label = label.replace(/[|;]/g, ' ');
        account_name = account_name.replace(/[|;]/g, ' ');
        secret = secret.replace(/[^A-Z2-7=]/g, '');
        result.push({ label: label, account_name: account_name, secret: secret, period: period, digits: digits });
      });
      return result;
    }

    function buildPayload(list) {
      return list.map(function(item) {
        return [item.label, item.account_name, item.secret, item.period, item.digits].join('|');
      }).join(';');
    }

    function parseOtpUri(text) {
      try {
        var url = new URL(text);
        if (url.protocol !== 'otpauth:') {
          return null;
        }
        if (url.hostname !== 'totp') {
          return null;
        }
        var label = decodeURIComponent(url.pathname.replace(/^\\//, '')) || 'TOTP';
        var params = new URLSearchParams(url.search);
        var secret = (params.get('secret') || '').replace(/\\s+/g, '').toUpperCase();
        if (!secret) {
          return null;
        }
        var period = parseInt(params.get('period') || '30', 10);
        if (!period || period < 1 || period > 120) {
          period = 30;
        }
        var digits = parseInt(params.get('digits') || '6', 10);
        if (!digits || digits < 6 || digits > 8) {
          digits = 6;
        }
        var issuer = params.get('issuer');
        var final_label = issuer || 'TOTP';
        var final_account_name = label;
        if (issuer && label.indexOf(':') === -1) {
          final_account_name = label;
        }
        final_label = final_label.replace(/[|;]/g, ' ');
        final_account_name = final_account_name.replace(/[|;]/g, ' ');
        return { label: final_label, account_name: final_account_name, secret: secret, period: period, digits: digits };
      } catch (err) {
        console.log('Invalid OTP URI', err);
        return null;
      }
    }

    document.getElementById('test-log').addEventListener('click', function() {
      console.log('[Totper] Test log button clicked');
      console.log('[Totper] Current entries:', entries);
      var list = readEntriesFromDom();
      console.log('[Totper] DOM entries:', list);
    });

    document.getElementById('add-entry').addEventListener('click', function() {
      entries.push({ label: '', account_name: '', secret: '', period: 30, digits: 6 });
      renderEntries();
    });

    document.getElementById('save').addEventListener('click', function() {
      console.log('[Totper] Save button clicked');
      var list = readEntriesFromDom();
      console.log('[Totper] Read entries:', list.length);
      if (!list.length) {
        alert('Add at least one entry first.');
        return;
      }
      var payload = buildPayload(list);
      console.log('[Totper] Built payload:', payload);
      var result = {
        entries: list,
        payload: payload
      };
      var url = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(result));
      console.log('[Totper] Closing with URL:', url);
      window.location = url;
    });

    document.getElementById('reset').addEventListener('click', function() {
      if (confirm('Delete all entries?')) {
        var result = { reset: true };
        window.location = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(result));
      }
    });

    renderEntries();
    console.log('[Totper] Initialization complete');
  })();
  </script>
</body>
</html>
`;

function buildConfigUrl(initialEntries) {
  const initialEncoded = encodeURIComponent(initialEntries || '');
  const htmlWithData = CONFIG_HTML.replace('__INITIAL_DATA__', initialEncoded);
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(htmlWithData);
}

function sendPayloadToWatch(payload) {
  console.log('[Totper] sendPayloadToWatch called with payload:', payload);
  return new Promise((resolve, reject) => {
    // Если payload пустой, отправляем команду очистки
    if (!payload || payload.trim() === '') {
      console.log('[Totper] Sending empty payload');
      Pebble.sendAppMessage(
        { AppKeyPayload: '' },
        () => {
          console.log('[Totper] Empty payload sent successfully');
          resolve();
        },
        err => {
          console.log('[Totper] Failed to send empty payload:', err);
          reject(err);
        }
      );
      return;
    }

    // Разбираем payload на отдельные записи
    const entries = payload.split(';').filter(entry => entry.trim() !== '');
    console.log('[Totper] Parsed entries:', entries);

    // Сначала отправляем количество записей
    console.log('[Totper] Sending count:', entries.length);
    Pebble.sendAppMessage(
      { AppKeyCount: entries.length },
      () => {
        console.log('[Totper] Count sent successfully');

        // Затем отправляем каждую запись отдельно с задержкой
        let sentCount = 0;
        const totalCount = entries.length;

        if (totalCount === 0) {
          resolve();
          return;
        }

        function sendNextEntry(index) {
          if (index >= totalCount) {
            console.log('[Totper] All entries sent, resolving');
            resolve();
            return;
          }

          console.log('[Totper] Sending entry', index, ':', entries[index].trim());
          Pebble.sendAppMessage(
            {
              AppKeyEntryId: index,
              AppKeyEntry: entries[index].trim()
            },
            () => {
              console.log('[Totper] Entry', index, 'sent successfully');
              sentCount++;
              // Отправляем следующий через 100ms
              setTimeout(() => sendNextEntry(index + 1), 100);
            },
            err => {
              console.log('[Totper] Failed to send entry', index, ':', err);
              reject(err);
            }
          );
        }

        // Начинаем отправку первой записи через 200ms после count
        setTimeout(() => sendNextEntry(0), 200);
      },
      err => reject(err)
    );
  });
}

function requestResend() {
  Pebble.sendAppMessage({ AppKeyRequest: 1 }, () => {}, () => {});
}

Pebble.addEventListener('ready', () => {
  console.log('Totper ready');
  // Не отправляем автоматически старые данные при запуске
  // Они будут отправлены только при явном сохранении
  requestResend();
});

Pebble.addEventListener('showConfiguration', () => {
  console.log('[Totper] showConfiguration called');
  const storedEntries = localStorage.getItem('totperConfigEntries') || '';
  console.log('[Totper] storedEntries:', storedEntries);
  const url = buildConfigUrl(storedEntries);
  console.log('[Totper] Opening URL:', url);
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', e => {
  console.log('[Totper] webviewclosed called', e);
  if (!e || !e.response) {
    console.log('[Totper] No response in webviewclosed');
    return;
  }
  let data;
  try {
    data = JSON.parse(decodeURIComponent(e.response));
    console.log('[Totper] Parsed data:', data);
  } catch (err) {
    console.log('Failed to parse config response', err);
    return;
  }

  if (data.reset) {
    localStorage.removeItem('totperConfigEntries');
    localStorage.removeItem('totperConfigPayload');
    sendPayloadToWatch('').catch(err => console.log('Failed to clear watch', err));
    return;
  }

  if (!Array.isArray(data.entries) || typeof data.payload !== 'string') {
    console.log('Config response invalid');
    return;
  }

  localStorage.setItem('totperConfigEntries', JSON.stringify(data.entries));
  localStorage.setItem('totperConfigPayload', data.payload);
  sendPayloadToWatch(data.payload).catch(err => {
    console.log('Failed to send payload', err);
  });
});

Pebble.addEventListener('appmessage', e => {
  console.log('[Totper] Received appmessage:', e);
  const payload = e.payload || {};
  if (payload.AppKeyRequest) {
    console.log('[Totper] Received AppKeyRequest - ignoring for unidirectional sync');
    // Для unidirectional sync не отправляем данные автоматически
  }
  if (payload.AppKeyStatus !== undefined) {
    console.log('[Totper] Watch status:', payload.AppKeyStatus);
  }
});

