// =====================================================================
// DRIFT landing page — small interactions (copy buttons + smooth anchors).
// =====================================================================

document.querySelectorAll('.copy-block').forEach(function (block) {
    var btn = block.querySelector('.copy-btn');
    if (!btn) return;
    btn.addEventListener('click', function () {
        var txt = block.getAttribute('data-copy') || block.innerText.replace(/copy$/i, '').trim();
        navigator.clipboard.writeText(txt).then(function () {
            var prev = btn.innerText;
            btn.innerText = 'copied';
            setTimeout(function () { btn.innerText = prev; }, 1300);
        }).catch(function () {});
    });
});
