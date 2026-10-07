#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <QTranslator>
#include <QString>
#include <QCoreApplication>

class MelonTranslator : public QTranslator
{
    Q_OBJECT
public:
    static MelonTranslator& Instance();
    static void Init(QCoreApplication* app);
    static void SetLanguage(const QString& lang);
    static QString GetLanguage();
    static QString Tr(const QString& text);
    static const char* TrC(const char* text);

    bool isEmpty() const override { return false; }
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;

private:
    MelonTranslator(QObject* parent = nullptr);
    QString currentLang;
};

#endif // TRANSLATION_H
