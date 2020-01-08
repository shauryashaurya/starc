#include "abstract_model.h"

#include <domain/document_object.h>

#include <utils/diff_match_patch/diff_match_patch_controller.h>


namespace BusinessLayer
{

class AbstractModel::Implementation
{
public:
    explicit Implementation(const QVector<QString>& _tags);


    /**
     * @brief Документ на основе которого строится модель
     */
    Domain::DocumentObject* document = nullptr;

    /**
     * @brief Контроллер для формирования патчей изменений документа
     */
    DiffMatchPatchController dmpController;
};

AbstractModel::Implementation::Implementation(const QVector<QString>& _tags)
    : dmpController(_tags)
{
}


// ****


AbstractModel::AbstractModel(const QVector<QString>& _tags, QObject* _parent)
    : QAbstractItemModel(_parent),
      d(new Implementation(_tags))
{

}

AbstractModel::~AbstractModel() = default;

Domain::DocumentObject* AbstractModel::document() const
{
    return d->document;
}

void AbstractModel::setDocument(Domain::DocumentObject* _document)
{
    if (d->document == _document) {
        return;
    }

    d->document = _document;

    initDocument();
}

void AbstractModel::clear()
{
    d->document = nullptr;

    clearDocument();
}

QModelIndex AbstractModel::index(int _row, int _column, const QModelIndex& _parent) const
{
    Q_UNUSED(_row)
    Q_UNUSED(_column)
    Q_UNUSED(_parent)

    return {};
}

QModelIndex AbstractModel::parent(const QModelIndex& _child) const
{
    Q_UNUSED(_child)

    return {};
}

int AbstractModel::columnCount(const QModelIndex& _parent) const
{
    Q_UNUSED(_parent)

    return {};
}

int AbstractModel::rowCount(const QModelIndex& _parent) const
{
    Q_UNUSED(_parent)

    return {};
}

QVariant AbstractModel::data(const QModelIndex& _index, int _role) const
{
    Q_UNUSED(_index)
    Q_UNUSED(_role)

    return {};
}

void AbstractModel::updateDocumentContent()
{
    if (d->document == nullptr) {
        return;
    }

    const auto content = toXml();

    const QByteArray undoPatch = d->dmpController.makePatch(content, d->document->content());
    if (undoPatch.isEmpty()) {
        return;
    }

    const QByteArray redoPatch = d->dmpController.makePatch(d->document->content(), content);
    if (redoPatch.isEmpty()) {
        return;
    }

    d->document->setContent(content);

    emit contentsChanged(undoPatch, redoPatch);
}

} // namespace BusinessLayer
