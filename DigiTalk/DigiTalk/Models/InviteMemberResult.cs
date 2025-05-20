// Запрос на добавление участника в групповой чат
namespace DigiTalk.Models
{
    public class InviteMemberResult
    {
        public bool Result { get; set; } // true - успех, false - ошибка
        public string Username { get; set; } // Имя пользователя
    }
}