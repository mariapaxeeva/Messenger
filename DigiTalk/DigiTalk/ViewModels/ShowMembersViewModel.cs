// Логика окна отображения участников группового чата
using ReactiveUI;
using System.Collections.Generic;
using System.Linq;
using System.Reactive.Linq;

namespace DigiTalk.ViewModels
{
    public class ShowMembersViewModel : ViewModelBase
    {
        private string _searchText = string.Empty;

        private List<string> _groupMembers; // Список участников группы
        public List<string> FilteredMembers { get; private set; } // Отфильтрованный список участников группы для поиска

        public string SearchText
        {
            get => _searchText;
            set
            {
                this.RaiseAndSetIfChanged(ref _searchText, value);
                FilterContacts();
            }
        }

        public ShowMembersViewModel(List<Contact> groupMembers)
        {
            _groupMembers = groupMembers.Select(c => c.Name).ToList();
            FilteredMembers = _groupMembers;
        }

        private void FilterContacts()
        {
            if (string.IsNullOrWhiteSpace(SearchText))
            {
                FilteredMembers = new List<string>(_groupMembers);
            }
            else
            {
                FilteredMembers = _groupMembers.Where(c => c.Contains(SearchText, System.StringComparison.OrdinalIgnoreCase)).ToList();
            }

            this.RaisePropertyChanged(nameof(FilteredMembers));
        }
    }
}