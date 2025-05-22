// Логика главного окна мессенджера
using System;
using System.Collections.ObjectModel;
using System.Net.Sockets;
using System.Reactive;
using System.Text;
using System.Threading;
using Avalonia.Controls;
using ReactiveUI;
using System.Threading.Tasks;
using DigiTalk.Models;
using DigiTalk.Views;
using System.Linq;
using System.Security.Cryptography;
using System.Reactive.Linq;
using System.IO;

namespace DigiTalk.ViewModels
{
    public class MainViewModel : ViewModelBase
    {
        private readonly Window _mainWindow;
        private TcpClient _client;
        private NetworkStream _stream;
        private string _username;
        private bool _isRunning;
        private Thread _receiveThread;

        private string _statusMessage = "Disconnected";
        public string StatusMessage
        {
            get => _statusMessage;
            set => this.RaiseAndSetIfChanged(ref _statusMessage, value);
        }

        private string _messageText;
        public string MessageText
        {
            get => _messageText;
            set => this.RaiseAndSetIfChanged(ref _messageText, value);
        }

        private Message _selectedMessage;
        public Message SelectedMessage
        {
            get => _selectedMessage;
            set => this.RaiseAndSetIfChanged(ref _selectedMessage, value);
        }

        private Contact _selectedContact;
        public Contact SelectedContact
        {
            get => _selectedContact;
            set
            {
                this.RaiseAndSetIfChanged(ref _selectedContact, value);
                Messages.Clear();
                if (value != null)
                {
                    LoadMessageHistory();
                }
            }
        }

        private bool _isAuthenticated;
        public bool IsAuthenticated
        {
            get => _isAuthenticated;
            set => this.RaiseAndSetIfChanged(ref _isAuthenticated, value);
        }
        private TaskCompletionSource<bool> _messageIdUpdatedTask = new TaskCompletionSource<bool>();
        public ObservableCollection<Contact> Contacts { get; } = new();
        public ObservableCollection<Message> Messages { get; } = new();
        public ObservableCollection<Contact> GroupMembers { get; } = new();
        public ReactiveCommand<Unit, Unit> SendMessageCommand { get; }
        public ReactiveCommand<Unit, Unit> RefreshContactsCommand { get; }
        public ReactiveCommand<Window, Unit> LoginCommand { get; }
        public ReactiveCommand<Unit, Unit> CreateGroupCommand { get; }
        public ReactiveCommand<Unit, Unit> InviteToGroupCommand { get; }
        public ReactiveCommand<Unit, Unit> LeaveGroupCommand { get; }
        public ReactiveCommand<Message, Unit> DeleteForMeCommand { get; }
        public ReactiveCommand<Message, Unit> DeleteForEveryoneCommand { get; }

        public MainViewModel(Window mainWindow)
        {
            _mainWindow = mainWindow;
            IsAuthenticated = false;

            // Инициализация команд
            SendMessageCommand = ReactiveCommand.Create(SendMessage);
            RefreshContactsCommand = ReactiveCommand.Create(RefreshContacts);
            LoginCommand = ReactiveCommand.CreateFromTask<Window>(LoginAsync);
            CreateGroupCommand = ReactiveCommand.CreateFromTask(CreateGroupAsync);
            InviteToGroupCommand = ReactiveCommand.CreateFromTask(InviteToGroupAsync);
            LeaveGroupCommand = ReactiveCommand.CreateFromTask(LeaveGroupAsync);
            DeleteForMeCommand = ReactiveCommand.CreateFromTask<Message>(DeleteForMe);
            DeleteForEveryoneCommand = ReactiveCommand.CreateFromTask<Message>(DeleteForEveryone);
        }

        private async Task LoginAsync(Window window)
        {
            var loginDialog = new LoginDialog();
            var result = await loginDialog.ShowDialog<LoginDialogResult>(window);

            if (result?.Result == true)
            {
                ConnectToServer(result.Username, result.Password, result.IsRegister);
            }
        }

        private void ConnectToServer(string username, string password, bool isRegister)
        {
            Disconnect();
            try
            {
                _username = username;
                _client = new TcpClient();
                _client.Connect("localhost", 8080);
                _stream = _client.GetStream();

                var authData = isRegister
                    ? $"REGISTER:{username}:{password}"
                    : $"{username}:{password}";
                SendMessageToStream(authData);

                var response = ReceiveMessageFromStream();
                if (isRegister)
                {
                    if (!response.StartsWith("OK"))
                    {
                        StatusMessage = $"Registration failed: {response}";
                        return;
                    }
                    StatusMessage = $"Registered successfully as {username}";
                }
                if (!response.StartsWith("OK"))
                {
                    StatusMessage = $"Connection failed: {response}";
                    return;
                }

                StatusMessage = $"Connected as {username}";
                IsAuthenticated = true;
                _isRunning = true;

                _receiveThread = new Thread(ReceiveMessages);
                _receiveThread.Start();
            }
            catch (Exception ex)
            {
                StatusMessage = $"Connection error: {ex.Message}";
            }
        }

        private void SendMessage()
        {
            if (SelectedContact == null || string.IsNullOrWhiteSpace(MessageText))
                return;

            try
            {
                var timestamp = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                var fullMessage = $"MSG:{(SelectedContact.IsGroup ? "#" : "")}{SelectedContact.Name}:{timestamp}:{MessageText}";
                SendMessageToStream(fullMessage);

                Messages.Add(new Message
                {
                    Sender = _username,
                    Content = MessageText,
                    Timestamp = DateTimeOffset.FromUnixTimeSeconds(timestamp).DateTime,
                    IsOwnMessage = true
                });

                MessageText = string.Empty;
            }
            catch (Exception ex)
            {
                StatusMessage = $"Send error: {ex.Message}";
            }
        }

        private void RefreshContacts()
        {
            SendMessageToStream($"GET_CONTACTS");
        }

        private void GetGroupMembers()
        {
            SendMessageToStream($"GET_GROUP_MEMBERS:{SelectedContact.Name}");
        }

        private void SendMessageToStream(string message)
        {
            var data = Encoding.UTF8.GetBytes(message);
            _stream.Write(data, 0, data.Length);
        }

        private string ReceiveMessageFromStream()
        {
            var buffer = new byte[4096];
            var bytesRead = _stream.Read(buffer, 0, buffer.Length);
            return Encoding.UTF8.GetString(buffer, 0, bytesRead);
        }

        private void ReceiveMessages()
        {
            try
            {
                while (_isRunning)
                {
                    try
                    {
                        var message = ReceiveMessageFromStream();

                        Avalonia.Threading.Dispatcher.UIThread.Post(() =>
                        {
                            if (message.StartsWith("CONTACTS:"))
                            {
                                ProcessContactsList(message);
                            }
                            else if (message.StartsWith("HISTORY:"))
                            {
                                ProcessHistoryMessages(message);
                            }
                            else if (message.StartsWith("MSG:"))
                            {
                                ProcessIncomingMessage(message);
                            }
                            else if (message.StartsWith("GROUP_CREATED:"))
                            {
                                // Формат: GROUP_CREATED:groupname
                                var groupName = message.Substring(14);
                                StatusMessage = $"Group successfully created: {groupName}";
                            }
                            else if (message.StartsWith("GROUP_MEMBER_ADDED:"))
                            {
                                // Формат: GROUP_MEMBER_ADDED:groupname:username
                                var parts = message.Substring(18).Split(':');
                                if (parts.Length == 2)
                                {
                                    var groupName = parts[0];
                                    var username = parts[1];
                                    StatusMessage = $"{username} joined group {groupName}";
                                }
                            }
                            else if (message.StartsWith("GROUP_MEMBER_LEFT:"))
                            {
                                // Формат: GROUP_MEMBER_LEFT:groupname:username
                                var parts = message.Substring(17).Split(':');
                                if (parts.Length == 2)
                                {
                                    var groupName = parts[0];
                                    var username = parts[1];
                                    StatusMessage = $"{username} left group {groupName}";
                                }
                            }
                            else if (message.StartsWith("GROUP_MEMBERS:"))
                            {
                                ProcessMembersList(message);
                            }
                        });
                    }
                    catch (IOException) when (!_isRunning)
                    {
                        break;
                    }
                    catch (Exception ex)
                    {
                        Avalonia.Threading.Dispatcher.UIThread.Post(() =>
                            StatusMessage = $"Receive error: {ex.Message}");
                            _isRunning = false;
                        break;
                    }
                }
            }
            finally
            {
                Avalonia.Threading.Dispatcher.UIThread.Post(() =>
                    StatusMessage = "Disconnected");
            }
        }

        private void ProcessContactsList(string message)
        {
            Contacts.Clear();

            // Формат: CONTACTS:username1,username2...|GROUPS:groupname1,groupname2...
            var parts = message.Split('|');
            var users = parts[0].Substring(9).Split(',', StringSplitOptions.RemoveEmptyEntries);
            var groups = parts[1].Substring(7).Split(',', StringSplitOptions.RemoveEmptyEntries);

            foreach (var user in users)
                Contacts.Add(new Contact { Name = user, IsGroup = false });

            foreach (var group in groups)
                Contacts.Add(new Contact { Name = group, IsGroup = true });
        }

        private void ProcessMembersList(string message)
        {
            GroupMembers.Clear();

            // Формат: GROUP_MEMBERS:groupname:member1,member2,member3
            var parts = message.Substring(14).Split(':');
            var groupName = parts[0];
            var members = parts[1].Split(',', StringSplitOptions.RemoveEmptyEntries).ToList();
            foreach (var member in members)
                GroupMembers.Add(new Contact { Name = member, IsGroup = false });
        }

        private void LoadMessageHistory()
        {
            if (SelectedContact == null) return;

            try
            {
                var request = $"GET_HISTORY:{SelectedContact.Name}";
                SendMessageToStream(request);
            }
            catch (Exception ex)
            {
                StatusMessage = $"History load error: {ex.Message}";
            }
        }

        private void ProcessHistoryMessages(string message)
        {
            Messages.Clear();

            // Формат: HISTORY:id0:sender0,timestamp0,is_group0,content0|id1,sender1...
            var historyItems = message.Substring(8).Split('|');
            foreach (var item in historyItems)
            {
                var parts = item.Split(':');
                if (parts.Length >= 5)
                {
                    var timestamp = DateTimeOffset.FromUnixTimeSeconds(long.Parse(parts[2])).DateTime;

                    Messages.Add(new Message
                    {
                        Id = int.Parse(parts[0]),
                        Sender = parts[1],
                        Content = string.Join(":", parts.Skip(4)), // На случай, если в сообщении есть ':'
                        Timestamp = timestamp,
                        IsGroupMessage = parts[3] == "1",
                        IsOwnMessage = parts[1] == _username
                    });
                }
            }
            // История загружена
            _messageIdUpdatedTask.TrySetResult(true);
        }

        private async Task CreateGroupAsync()
        {
            // Фильтр контактов (исключает текущего пользователя и группы)
            var filteredContacts = Contacts
                .Where(c => c.Name != _username && !c.IsGroup)
                .ToList();

            var dialog = new CreateGroupDialog()
            {
                DataContext = new GroupDialogViewModel(filteredContacts)
            };
            var result = await dialog.ShowDialog<GroupCreationResult>(_mainWindow); 
            if (result?.Result == true)
            {
                // Формирование команды для сервера: CREATE_GROUP:groupname:member1,member2,member3
                var members = string.Join(",", result.MemberUsernames);
                var command = $"CREATE_GROUP:{result.Groupname}:{members}";
                SendMessageToStream(command);

                // Локальное добавление группы в список контактов
                Contacts.Add(new Contact
                {
                    Name = result.Groupname,
                    IsGroup = true
                });
            }
        }
        private void ProcessIncomingMessage(string message)
        {
            var parts = message.Substring(4).Split(':');
            if (parts.Length >= 3)
            {
                var timestamp = DateTimeOffset.FromUnixTimeSeconds(long.Parse(parts[1])).DateTime;
                var sender = parts[0];
                var content = string.Join(":", parts.Skip(2));
                var isGroup = sender.StartsWith("#"); // Групповые сообщения имеют префикс #

                Messages.Add(new Message
                {
                    Sender = sender,
                    Content = content,
                    Timestamp = timestamp,
                    IsOwnMessage = false,
                    IsGroupMessage = isGroup
                });
            }
        }

        private async Task InviteToGroupAsync()
        {
            if (SelectedContact?.IsGroup != true) return;

            // Создание наблюдаемой последовательности для ожидания обновления GroupMembers
            var membersUpdated = this.WhenAnyValue(x => x.GroupMembers.Count)
                                    .Skip(1)
                                    .FirstAsync()
                                    .Timeout(TimeSpan.FromSeconds(5))
                                    .Catch(Observable.Return(0));

            // Запрос участников группы у сервера
            GetGroupMembers();

            try
            {
                // Ожидание обновления GroupMembers
                await membersUpdated;

                var dialog = new InviteMemberDialog()
                {
                    DataContext = new InviteMemberViewModel(Contacts.ToList(), GroupMembers.ToList())
                };
                var result = await dialog.ShowDialog<InviteMemberResult>(_mainWindow);
                if (result?.Result == true)
                {
                    var command = $"INVITE_TO_GROUP:{SelectedContact.Name}:{result.Username}";
                    SendMessageToStream(command);
                }
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
            }
        }

        private async Task LeaveGroupAsync()
        {
            if (SelectedContact?.IsGroup != true) return;

            SendMessageToStream($"LEAVE_GROUP:{SelectedContact.Name}");
            Contacts.Remove(SelectedContact);
            SelectedContact = null;
        }

        private async Task DeleteMessage(Message message)
        {
            if (message == null) return;

            try
            {
                // Если message.Id == 0, загрузка истории сообщений
                if (message.Id == 0)
                {
                    _messageIdUpdatedTask = new TaskCompletionSource<bool>();
                    LoadMessageHistory();
                    await _messageIdUpdatedTask.Task;

                    // Поиск сообщения в обновленной коллекции
                    var updatedMessage = Messages.FirstOrDefault(m =>
                        m.Sender == message.Sender &&
                        m.Content == message.Content &&
                        m.Timestamp == message.Timestamp);

                    if (updatedMessage == null) return;
                    message = updatedMessage;
                }

                // Формирование команды для сервера
                var command = message.IsDeletedForEveryone ? $"DEL:{message.Id}:1" : $"DEL:{message.Id}:0";
                SendMessageToStream(command);

                // Локальное удаление
                Messages.Remove(message);

                // Уведомление UI об изменении
                this.RaisePropertyChanged(nameof(Messages));
            }
            catch (Exception ex)
            {
                StatusMessage = $"Delete error: {ex.Message}";
            }
        }

        private async Task DeleteForMe(Message message)
        {
            message.IsDeletedForEveryone = false;
            await DeleteMessage(message);
        }
        private async Task DeleteForEveryone(Message message)
        {
            message.IsDeletedForEveryone = true;
            await DeleteMessage(message);
        }

        public void Disconnect()
        {
            try
            {
                _isRunning = false;
                IsAuthenticated = false;
                Contacts.Clear();

                _client?.Client?.Close(); // Прерывание блокирующей операции чтения (принудительное закрытие сокета)

                // Ожидание завершения потока (с таймаутом 2 сек)
                if (_receiveThread != null && _receiveThread.IsAlive)
                {
                    if (!_receiveThread.Join(TimeSpan.FromSeconds(2)))
                    {
                        _receiveThread.Interrupt(); // Принудительное прерывание
                    }
                }
                _stream?.Close();
                _client?.Close();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Disconnect error: {ex.Message}");
            }
            finally
            {
                StatusMessage = "Disconnected";
            }
        }
    }
}