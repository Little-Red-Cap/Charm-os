module;
export module gui.qr_encode;

export import alg.qr_encode;

// Compatibility namespace: keep existing gui::qr users working.
export namespace gui::qr = alg::qr;
