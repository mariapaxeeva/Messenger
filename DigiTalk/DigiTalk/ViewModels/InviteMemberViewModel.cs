// Логика окна добавления участника в групповой чат
using ReactiveUI;
using System.Collections.Generic;
using System.Linq;
using System.Reactive.Linq;

namespace DigiTalk.ViewModels
{
    public class InviteMemberViewModel : ViewModelBase
    {
        private string _searchText = string.Empty;
        private Contact _selectedContact;
        private List<string> _groupMembers; // Список участников группы

        public List<Contact> AvailableContacts { get; }
        public List<Contact> FilteredContacts { get; private set; }

        public string SearchText
        {
            get => _searchText;
            set
            {
                this.RaiseAndSetIfChanged(ref _searchText, value);
                FilterContacts();
            }
        }

        public Contact SelectedContact
        {
            get => _selectedContact;
            set => this.RaiseAndSetIfChanged(ref _selectedContact, value);
        }

        public InviteMemberViewModel(List<Contact> contacts, List<Contact> groupMembers)
        {
            _groupMembers = groupMembers.Select(c => c.Name).ToList();
            // Исключение текущего пользователя, групп и участников текущей группы
            AvailableContacts = contacts
                .Where(c => !c.IsGroup && !_groupMembers.Contains(c.Name))
                .ToList();
            FilteredContacts = AvailableContacts;
        }

        private void FilterContacts()
        {
            if (string.IsNullOrWhiteSpace(SearchText))
            {
                FilteredContacts = new List<Contact>(AvailableContacts);
            }
            else
            {
                FilteredContacts = AvailableContacts
                    .Where(c => c.Name.Contains(SearchText, System.StringComparison.OrdinalIgnoreCase))
                    .ToList();
            }

            this.RaisePropertyChanged(nameof(FilteredContacts));
        }
    }
}