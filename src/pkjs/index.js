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
    <button id="add-entry" class="secondary">Add Entry</button>
    <button id="save" class="primary">Save</button>
    <button id="reset" class="danger">Clear All</button>
  </div>

  <script>
  (function() {
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
        '<label>Account Name</label>',
        '<input type="text" name="name" placeholder="e.g. Email" value="' + (entry.name || '') + '" maxlength="32" required>',
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
        var name = node.querySelector('input[name="name"]').value.trim();
        var secret = node.querySelector('input[name="secret"]').value.replace(/\\s+/g, '').toUpperCase();
        var period = parseInt(node.querySelector('input[name="period"]').value, 10);
        var digits = parseInt(node.querySelector('input[name="digits"]').value, 10);
        if (!name || !secret) {
          return;
        }
        if (!period || period < 1 || period > 120) {
          period = 30;
        }
        if (!digits || digits < 6 || digits > 8) {
          digits = 6;
        }
        name = name.replace(/[|;]/g, ' ');
        secret = secret.replace(/[^A-Z2-7=]/g, '');
        result.push({ name: name, secret: secret, period: period, digits: digits });
      });
      return result;
    }

    function buildPayload(list) {
      return list.map(function(item) {
        return [item.name, item.secret, item.period, item.digits].join('|');
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
        if (issuer && label.indexOf(':') === -1) {
          label = issuer + ' (' + label + ')';
        }
        label = label.replace(/[|;]/g, ' ');
        return { name: label, secret: secret, period: period, digits: digits };
      } catch (err) {
        console.log('Invalid OTP URI', err);
        return null;
      }
    }

    document.getElementById('add-entry').addEventListener('click', function() {
      entries.push({ name: '', secret: '', period: 30, digits: 6 });
      renderEntries();
    });

    document.getElementById('save').addEventListener('click', function() {
      var list = readEntriesFromDom();
      if (!list.length) {
        alert('Add at least one entry first.');
        return;
      }
      var payload = buildPayload(list);
      var result = {
        entries: list,
        payload: payload
      };
      window.location = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(result));
    });

    document.getElementById('reset').addEventListener('click', function() {
      if (confirm('Delete all entries?')) {
        var result = { reset: true };
        window.location = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(result));
      }
    });

    renderEntries();
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
  return new Promise((resolve, reject) => {
    Pebble.sendAppMessage(
      { AppKeyPayload: payload },
      () => resolve(),
      err => reject(err)
    );
  });
}

function requestResend() {
  Pebble.sendAppMessage({ AppKeyRequest: 1 }, () => {}, () => {});
}

Pebble.addEventListener('ready', () => {
  console.log('Totper ready');
  const storedPayload = localStorage.getItem('totperConfigPayload') || '';
  if (storedPayload) {
    sendPayloadToWatch(storedPayload).catch(err => {
      console.log('Failed to send stored payload', err);
    });
  } else {
    requestResend();
  }
});

Pebble.addEventListener('showConfiguration', () => {
  const storedEntries = localStorage.getItem('totperConfigEntries') || '';
  Pebble.openURL(buildConfigUrl(storedEntries));
});

Pebble.addEventListener('webviewclosed', e => {
  if (!e || !e.response) {
    return;
  }
  let data;
  try {
    data = JSON.parse(decodeURIComponent(e.response));
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
  const payload = e.payload || {};
  if (payload.AppKeyRequest) {
    const storedPayload = localStorage.getItem('totperConfigPayload') || '';
    if (storedPayload) {
      sendPayloadToWatch(storedPayload).catch(err => console.log('sync failed', err));
    }
  }
  if (payload.AppKeyStatus !== undefined) {
    console.log('Watch status: ' + payload.AppKeyStatus);
  }
});

