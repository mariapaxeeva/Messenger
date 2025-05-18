// Логика окна для создания группового чата
using ReactiveUI;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace DigiTalk.ViewModels
{
    public class GroupDialogViewModel : ViewModelBase
    {
        private string _groupName;
        public string Groupname
        {
            get => _groupName;
            set => this.RaiseAndSetIfChanged(ref _groupName, value);
        }

        // Cписок контактов, которые можно добавлять в групповой чат (нет групп и создателя текущего чата)
        public ObservableCollection<SelectableContact> Contacts { get; } = new();

        public GroupDialogViewModel(IEnumerable<Contact> contacts)
        {
            foreach (var contact in contacts)
            {
                Contacts.Add(new SelectableContact(contact));
            }
        }
    }

    public class SelectableContact : ViewModelBase
    {
        public Contact Contact { get; }
        private bool _isSelected;
        public bool IsSelected
        {
            get => _isSelected;
            set => this.RaiseAndSetIfChanged(ref _isSelected, value);
        }

        public SelectableContact(Contact contact)
        {
            Contact = contact;
        }
    }
}