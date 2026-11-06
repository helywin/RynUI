#include <ryn/rynui.hpp>

void qrcode_public_api_examples() {
    using namespace ryn;
    Signal<QRCodeValue> content{QRCodeValue{u8"RynUI"}};
    Signal<QRCodeStatus> status{QRCodeStatus::Expired};
    Signal<QRCodeErrorLevel> level{QRCodeErrorLevel::Q};
    QRCodeRef reference;
    auto renderer = std::make_shared<QRCodeStatusRenderer>();
    renderer->render = [](const QRCodeStatusContext& context) {
        return QRCodeStatusContent{[context] {
            Button(ButtonProps{}.disabled(context.disabled).onClick([context] { (void)context.refresh(); }),
                   ButtonContent{[context] {
                       Text(TextProps{}.content(bind([context] { return context.locale.get().refresh; })));
                   }});
        }};
    };
    QRCode(QRCodeProps{}
               .value(content)
               .status(status)
               .errorLevel(level)
               .boostLevel(false)
               .marginSize(4)
               .size(192)
               .ref(reference)
               .statusRender(renderer)
               .onRefresh([status] { status.set(QRCodeStatus::Active); }));
    (void)reference.data();
    (void)reference.exportPng(384);
    (void)reference.exportSvg();
    ThemeConfig theme;
    theme.qrcode.tokens.foreground = Color::rgba8(0, 0, 0);
    theme.qrcode.tokens.background = Color::rgba8(255, 255, 255);
    theme.qrcode.tokens.radius = dp(8);
}
