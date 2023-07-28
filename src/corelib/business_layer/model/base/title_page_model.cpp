#include "title_page_model.h"


namespace BusinessLayer {

class TitlePageModel::Implementation
{
public:
    /**
     * @brief Список персонажей сценария
     */
    QVector<QPair<QString, QString>> characters;
};


// ****


TitlePageModel::TitlePageModel(QObject* _parent)
    : SimpleTextModel(_parent)
    , d(new Implementation)
{
}

TitlePageModel::~TitlePageModel() = default;

void TitlePageModel::setDocumentName(const QString& _name)
{
    Q_UNUSED(_name);
}

void TitlePageModel::setCharacters(const QVector<QPair<QString, QString>>& _characters)
{
    d->characters = _characters;
}

QVector<QPair<QString, QString>> TitlePageModel::characters() const
{
    return d->characters;
}

} // namespace BusinessLayer
